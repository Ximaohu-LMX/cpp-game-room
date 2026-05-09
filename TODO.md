下面按一个**C++ 房间制多人对战游戏服务端**来设计。

目标不是商业级大服，而是一个可以写进简历、可以压测、可以讲清楚的项目。

默认技术栈：

```text
C++17 / C++20
Boost.Asio 或 muduo / libhv
protobuf
Redis
MySQL
spdlog
CMake
Docker Compose
GoogleTest
```

项目名可以叫：

```text
cpp-game-server
```

---

# 一、整体目录结构

建议你按这个结构做：

```text
cpp-game-server/
├── CMakeLists.txt
├── README.md
├── docker-compose.yml
├── config/
│   ├── server.yaml
│   ├── mysql.sql
│   └── redis.conf
│
├── proto/
│   ├── common.proto
│   ├── login.proto
│   ├── match.proto
│   ├── room.proto
│   ├── game.proto
│   └── rank.proto
│
├── src/
│   ├── main.cpp
│   ├── server/
│   ├── net/
│   ├── protocol/
│   ├── player/
│   ├── login/
│   ├── match/
│   ├── room/
│   ├── game/
│   ├── storage/
│   ├── rank/
│   ├── timer/
│   ├── util/
│   └── config/
│
├── bot/
│   ├── CMakeLists.txt
│   ├── bot_main.cpp
│   ├── bot_client.h
│   ├── bot_client.cpp
│   ├── bot_manager.h
│   └── bot_manager.cpp
│
├── tests/
│   ├── test_room_state.cpp
│   ├── test_match_queue.cpp
│   ├── test_protocol.cpp
│   └── test_settlement.cpp
│
└── docs/
    ├── architecture.md
    ├── protocol.md
    ├── pressure_test.md
    └── interview_notes.md
```

---

# 二、核心模块关系

整体逻辑是：

```text
Client / Bot
   |
   v
Network 层
   |
   v
Protocol 协议层
   |
   v
MessageDispatcher 消息分发
   |
   +--> LoginService
   +--> MatchService
   +--> RoomManager
   +--> GameLoop / GameRoom
   +--> RankService
   +--> StorageService
```

也就是说，客户端发来的每个请求，先经过：

```text
TCP 收包 -> 解码 -> 根据 msg_id 分发 -> 业务处理 -> 编码 -> 发回客户端
```

你面试时一定要能画出这条链路。

---

# 三、Build 和配置模块

## 1. 根目录文件

### `CMakeLists.txt`

功能：

* 管理整个项目编译；
* 引入 protobuf；
* 引入 Boost.Asio / muduo / libhv；
* 引入 spdlog；
* 编译 server、bot、tests。

建议功能：

```cmake
add_subdirectory(src)
add_subdirectory(bot)
add_subdirectory(tests)
```

---

### `README.md`

功能：

* 项目介绍；
* 架构图；
* 快速启动；
* 功能列表；
* 压测数据；
* 面试亮点。

README 很重要。小厂面试官可能会直接看你的 README。

建议结构：

```text
1. 项目简介
2. 技术栈
3. 架构设计
4. 核心功能
5. 协议设计
6. 房间状态机
7. 状态同步流程
8. Redis / MySQL 数据设计
9. 压测结果
10. 常见问题与优化
```

---

### `docker-compose.yml`

功能：

* 启动 MySQL；
* 启动 Redis；
* 可选启动 server。

示例组件：

```text
mysql
redis
game-server
```

---

## 2. `config/`

### `config/server.yaml`

功能：

保存服务端配置。

内容例如：

```yaml
server:
  host: "0.0.0.0"
  port: 9000
  worker_threads: 4
  tick_interval_ms: 50

mysql:
  host: "127.0.0.1"
  port: 3306
  user: "root"
  password: "123456"
  database: "game"

redis:
  host: "127.0.0.1"
  port: 6379

match:
  room_player_count: 2
```

---

### `config/mysql.sql`

功能：

初始化数据库表。

建议至少有这几张表：

```sql
CREATE TABLE player (
    player_id BIGINT PRIMARY KEY,
    name VARCHAR(64),
    score INT DEFAULT 1000,
    win_count INT DEFAULT 0,
    lose_count INT DEFAULT 0,
    create_time TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    update_time TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP
);

CREATE TABLE battle_record (
    battle_id BIGINT PRIMARY KEY,
    room_id BIGINT,
    winner_id BIGINT,
    loser_id BIGINT,
    start_time TIMESTAMP,
    end_time TIMESTAMP,
    result_json TEXT
);

CREATE TABLE settlement_log (
    settlement_id BIGINT PRIMARY KEY,
    battle_id BIGINT,
    player_id BIGINT,
    score_delta INT,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    UNIQUE KEY uniq_battle_player (battle_id, player_id)
);
```

重点是 `settlement_log`，它可以用来讲**结算幂等**。

---

# 四、协议模块 `proto/`

协议建议用 protobuf。不要用裸 JSON 作为主协议，JSON 可以用于调试。

## 1. `proto/common.proto`

功能：

定义通用结构。

```proto
syntax = "proto3";

package proto;

message Vector2 {
  float x = 1;
  float y = 2;
}

message PlayerInfo {
  int64 player_id = 1;
  string name = 2;
  int32 score = 3;
}

message ErrorInfo {
  int32 code = 1;
  string message = 2;
}
```

---

## 2. `proto/login.proto`

功能：

登录、心跳、断线重连。

包含消息：

```proto
message LoginRequest {
  string account = 1;
  string token = 2;
}

message LoginResponse {
  int32 code = 1;
  int64 player_id = 2;
  string name = 3;
}

message HeartbeatRequest {
  int64 client_time_ms = 1;
}

message HeartbeatResponse {
  int64 server_time_ms = 1;
}

message ReconnectRequest {
  int64 player_id = 1;
  string session_token = 2;
}

message ReconnectResponse {
  int32 code = 1;
  int64 room_id = 2;
}
```

面试可讲：

* 登录后如何绑定连接和玩家；
* 重复登录怎么处理；
* 心跳超时怎么踢人；
* 断线重连如何恢复房间状态。

---

## 3. `proto/match.proto`

功能：

匹配相关协议。

```proto
message MatchRequest {
  int64 player_id = 1;
}

message MatchCancelRequest {
  int64 player_id = 1;
}

message MatchResponse {
  int32 code = 1;
  string message = 2;
}

message MatchSuccessNotify {
  int64 room_id = 1;
  repeated int64 player_ids = 2;
}
```

---

## 4. `proto/room.proto`

功能：

房间准备、进入、退出、状态通知。

```proto
message EnterRoomRequest {
  int64 room_id = 1;
}

message EnterRoomResponse {
  int32 code = 1;
  int64 room_id = 2;
}

message ReadyRequest {
  bool ready = 1;
}

message ReadyNotify {
  int64 player_id = 1;
  bool ready = 2;
}

message RoomStateNotify {
  int64 room_id = 1;
  int32 state = 2;
  repeated int64 player_ids = 3;
}
```

---

## 5. `proto/game.proto`

功能：

游戏输入、状态同步、结算。

```proto
message PlayerInput {
  int64 player_id = 1;
  int64 input_seq = 2;
  float move_x = 3;
  float move_y = 4;
  bool fire = 5;
}

message InputRequest {
  PlayerInput input = 1;
}

message PlayerState {
  int64 player_id = 1;
  float x = 2;
  float y = 3;
  int32 hp = 4;
}

message GameStateNotify {
  int64 room_id = 1;
  int64 frame_id = 2;
  repeated PlayerState players = 3;
}

message GameOverNotify {
  int64 room_id = 1;
  int64 winner_id = 2;
  repeated int64 loser_ids = 3;
}
```

---

## 6. `proto/rank.proto`

功能：

排行榜协议。

```proto
message RankRequest {
  int32 offset = 1;
  int32 limit = 2;
}

message RankItem {
  int64 player_id = 1;
  string name = 2;
  int32 score = 3;
  int32 rank = 4;
}

message RankResponse {
  repeated RankItem items = 1;
}
```

---

# 五、主程序模块

## `src/main.cpp`

功能：

* 加载配置；
* 初始化日志；
* 初始化 Redis / MySQL；
* 初始化各个 service；
* 启动 server；
* 进入事件循环。

伪代码：

```cpp
int main() {
    ConfigManager::Instance().Load("config/server.yaml");
    Logger::Init();

    StorageService storage;
    storage.Init();

    RankService rank;
    rank.Init();

    GameServer server;
    server.Init();
    server.Start();

    return 0;
}
```

---

# 六、Server 总控模块 `src/server/`

目录：

```text
src/server/
├── game_server.h
├── game_server.cpp
├── service_context.h
├── service_context.cpp
├── message_dispatcher.h
└── message_dispatcher.cpp
```

---

## 1. `game_server.h / game_server.cpp`

功能：

`GameServer` 是服务端总入口。

负责：

* 启动 TCP Server；
* 初始化业务模块；
* 注册消息处理函数；
* 持有全局上下文；
* 管理服务生命周期。

核心类：

```cpp
class GameServer {
public:
    bool Init();
    void Start();
    void Stop();

private:
    std::unique_ptr<TcpServer> tcp_server_;
    std::unique_ptr<ServiceContext> context_;
    std::unique_ptr<MessageDispatcher> dispatcher_;
};
```

---

## 2. `service_context.h / service_context.cpp`

功能：

保存所有业务服务的引用，避免到处用全局变量。

里面可以放：

```cpp
class ServiceContext {
public:
    LoginService* login_service;
    MatchService* match_service;
    RoomManager* room_manager;
    RankService* rank_service;
    StorageService* storage_service;
    PlayerManager* player_manager;
};
```

面试可讲：

> 我用 ServiceContext 管理模块依赖，避免 service 之间直接 new 或全局单例乱飞。

---

## 3. `message_dispatcher.h / message_dispatcher.cpp`

功能：

根据 `msg_id` 分发消息。

比如：

```text
MSG_LOGIN_REQ      -> LoginService::HandleLogin
MSG_MATCH_REQ      -> MatchService::HandleMatch
MSG_READY_REQ      -> RoomService::HandleReady
MSG_INPUT_REQ      -> GameRoom::HandleInput
MSG_RANK_REQ       -> RankService::HandleRank
```

核心结构：

```cpp
using Handler = std::function<void(const SessionPtr&, const Packet&)>;

class MessageDispatcher {
public:
    void RegisterHandler(uint32_t msg_id, Handler handler);
    void Dispatch(const SessionPtr& session, const Packet& packet);

private:
    std::unordered_map<uint32_t, Handler> handlers_;
};
```

面试重点：

* 消息如何从网络层到业务层；
* 未注册 msg_id 怎么处理；
* handler 是否线程安全。

---

# 七、网络模块 `src/net/`

目录：

```text
src/net/
├── tcp_server.h
├── tcp_server.cpp
├── tcp_connection.h
├── tcp_connection.cpp
├── session.h
├── session.cpp
├── packet.h
├── packet.cpp
├── codec.h
├── codec.cpp
└── connection_manager.h
```

---

## 1. `tcp_server.h / tcp_server.cpp`

功能：

* 监听端口；
* accept 新连接；
* 创建 `TcpConnection`；
* 设置读写回调。

核心类：

```cpp
class TcpServer {
public:
    bool Start(const std::string& host, uint16_t port);
    void Stop();

private:
    void OnAccept();
};
```

---

## 2. `tcp_connection.h / tcp_connection.cpp`

功能：

代表一个 TCP 连接。

负责：

* 异步读；
* 异步写；
* 连接关闭；
* 收到数据后交给 codec；
* 发送数据包。

核心类：

```cpp
class TcpConnection {
public:
    void Start();
    void Send(const Packet& packet);
    void Close();

    uint64_t ConnId() const;

private:
    void DoRead();
    void DoWrite();

private:
    uint64_t conn_id_;
    std::vector<char> read_buffer_;
    std::deque<std::string> write_queue_;
};
```

---

## 3. `session.h / session.cpp`

功能：

`Session` 是业务层看到的连接对象。

它比 `TcpConnection` 更贴业务：

```cpp
class Session {
public:
    void Send(uint32_t msg_id, const google::protobuf::Message& msg);
    void Close();

    void BindPlayerId(int64_t player_id);
    int64_t PlayerId() const;

    void SetRoomId(int64_t room_id);
    int64_t RoomId() const;

    void UpdateHeartbeatTime();

private:
    uint64_t session_id_;
    int64_t player_id_;
    int64_t room_id_;
    std::weak_ptr<TcpConnection> conn_;
    int64_t last_heartbeat_ms_;
};
```

面试高频问题：

> 为什么要有 Session，而不是直接用 TcpConnection？

回答：

> TcpConnection 只关心网络连接生命周期，Session 关心玩家身份、房间、心跳和业务状态。连接断开后，Session 可以短时间保留，用于断线重连。

---

## 4. `packet.h / packet.cpp`

功能：

定义网络包格式。

建议协议头：

```text
| length 4 bytes | msg_id 4 bytes | seq 4 bytes | body |
```

对应结构：

```cpp
struct Packet {
    uint32_t length;
    uint32_t msg_id;
    uint32_t seq;
    std::string body;
};
```

功能：

* 序列化 packet；
* 解析 packet；
* 校验长度；
* 防止非法大包。

---

## 5. `codec.h / codec.cpp`

功能：

解决 TCP 粘包拆包。

核心功能：

```cpp
class Codec {
public:
    std::vector<Packet> Decode(Buffer& buffer);
    std::string Encode(const Packet& packet);
};
```

面试重点：

* TCP 是字节流；
* 为什么会粘包拆包；
* 如何根据 length 字段拆包；
* 最大包长如何限制。

---

## 6. `connection_manager.h`

功能：

管理所有在线连接。

```cpp
class ConnectionManager {
public:
    void Add(SessionPtr session);
    void Remove(uint64_t session_id);
    SessionPtr Get(uint64_t session_id);
    SessionPtr GetByPlayerId(int64_t player_id);

private:
    std::unordered_map<uint64_t, SessionPtr> sessions_;
    std::unordered_map<int64_t, SessionPtr> player_sessions_;
};
```

用途：

* 广播；
* 踢下线；
* 重复登录处理；
* 查询玩家连接。

---

# 八、协议封装模块 `src/protocol/`

目录：

```text
src/protocol/
├── message_id.h
├── proto_helper.h
└── proto_helper.cpp
```

---

## 1. `message_id.h`

功能：

定义所有消息 ID。

```cpp
enum MessageId {
    MSG_LOGIN_REQ = 1001,
    MSG_LOGIN_RESP = 1002,

    MSG_HEARTBEAT_REQ = 1003,
    MSG_HEARTBEAT_RESP = 1004,

    MSG_MATCH_REQ = 2001,
    MSG_MATCH_RESP = 2002,
    MSG_MATCH_SUCCESS_NOTIFY = 2003,

    MSG_READY_REQ = 3001,
    MSG_READY_NOTIFY = 3002,

    MSG_INPUT_REQ = 4001,
    MSG_GAME_STATE_NOTIFY = 4002,
    MSG_GAME_OVER_NOTIFY = 4003,

    MSG_RANK_REQ = 5001,
    MSG_RANK_RESP = 5002,
};
```

---

## 2. `proto_helper.h / proto_helper.cpp`

功能：

封装 protobuf 的序列化和反序列化。

```cpp
class ProtoHelper {
public:
    template<typename T>
    static bool Parse(const Packet& packet, T* msg);

    static Packet Build(uint32_t msg_id, const google::protobuf::Message& msg);
};
```

---

# 九、玩家模块 `src/player/`

目录：

```text
src/player/
├── player.h
├── player.cpp
├── player_manager.h
└── player_manager.cpp
```

---

## 1. `player.h / player.cpp`

功能：

表示一个玩家对象。

```cpp
enum class PlayerStatus {
    Offline,
    Online,
    Matching,
    InRoom,
    Playing
};

class Player {
public:
    int64_t Id() const;
    const std::string& Name() const;

    void SetStatus(PlayerStatus status);
    PlayerStatus Status() const;

    void SetScore(int score);
    int Score() const;

private:
    int64_t player_id_;
    std::string name_;
    int score_;
    PlayerStatus status_;
    int64_t room_id_;
};
```

---

## 2. `player_manager.h / player_manager.cpp`

功能：

管理在线玩家和玩家状态。

```cpp
class PlayerManager {
public:
    PlayerPtr GetOrCreatePlayer(int64_t player_id);
    PlayerPtr GetPlayer(int64_t player_id);

    void SetOnline(int64_t player_id, SessionPtr session);
    void SetOffline(int64_t player_id);

    bool IsOnline(int64_t player_id);

private:
    std::unordered_map<int64_t, PlayerPtr> players_;
};
```

面试重点：

* 玩家对象生命周期；
* 断线后是否立刻删除；
* 重连如何恢复状态。

---

# 十、登录模块 `src/login/`

目录：

```text
src/login/
├── login_service.h
└── login_service.cpp
```

---

## `login_service.h / login_service.cpp`

功能：

处理：

* 登录；
* 重复登录；
* 心跳；
* 断线重连；
* 玩家数据加载。

核心类：

```cpp
class LoginService {
public:
    void RegisterHandlers(MessageDispatcher& dispatcher);

    void HandleLogin(const SessionPtr& session, const Packet& packet);
    void HandleHeartbeat(const SessionPtr& session, const Packet& packet);
    void HandleReconnect(const SessionPtr& session, const Packet& packet);

private:
    std::string GenerateSessionToken(int64_t player_id);
};
```

登录流程：

```text
收到 LoginRequest
解析 account/token
查询或创建 player_id
加载玩家数据
绑定 session 和 player
返回 LoginResponse
```

重复登录策略：

```text
如果 player_id 已在线：
    关闭旧 session
    新 session 绑定 player
```

面试重点：

* 重复登录怎么处理；
* session 和 player 怎么绑定；
* 断线重连怎么恢复房间；
* 心跳超时谁来检测。

---

# 十一、匹配模块 `src/match/`

目录：

```text
src/match/
├── match_service.h
├── match_service.cpp
├── match_queue.h
└── match_queue.cpp
```

---

## 1. `match_service.h / match_service.cpp`

功能：

处理客户端匹配请求。

```cpp
class MatchService {
public:
    void RegisterHandlers(MessageDispatcher& dispatcher);

    void HandleMatch(const SessionPtr& session, const Packet& packet);
    void HandleCancelMatch(const SessionPtr& session, const Packet& packet);

private:
    void TryCreateRoom();
};
```

流程：

```text
玩家请求匹配
检查玩家是否在线
检查是否已经在房间/匹配中
加入匹配队列
如果队列人数足够，创建房间
通知玩家匹配成功
```

---

## 2. `match_queue.h / match_queue.cpp`

功能：

维护匹配队列。

```cpp
class MatchQueue {
public:
    bool Push(int64_t player_id);
    bool Remove(int64_t player_id);
    bool Contains(int64_t player_id) const;

    std::vector<int64_t> PopN(size_t n);
    size_t Size() const;

private:
    std::deque<int64_t> queue_;
    std::unordered_set<int64_t> in_queue_;
};
```

为什么同时用 `deque` 和 `unordered_set`？

* `deque` 保证匹配顺序；
* `unordered_set` 防止重复入队；
* 取消匹配时可以判断是否在队列中。

面试很容易问到这个点。

---

# 十二、房间模块 `src/room/`

目录：

```text
src/room/
├── room.h
├── room.cpp
├── room_manager.h
├── room_manager.cpp
├── room_state.h
└── room_player.h
```

---

## 1. `room_state.h`

功能：

定义房间状态。

```cpp
enum class RoomState {
    Waiting,
    Ready,
    Playing,
    Settlement,
    Closed
};
```

状态机：

```text
Waiting -> Ready -> Playing -> Settlement -> Closed
```

你可以在 README 里画这张图。

---

## 2. `room_player.h`

功能：

房间内玩家信息。

```cpp
struct RoomPlayer {
    int64_t player_id;
    bool ready = false;
    bool connected = true;
    int32_t hp = 100;
    float x = 0;
    float y = 0;
};
```

---

## 3. `room.h / room.cpp`

功能：

单个房间对象。

负责：

* 玩家加入；
* 玩家退出；
* 准备；
* 开局；
* 结算；
* 广播房间消息。

核心类：

```cpp
class Room {
public:
    explicit Room(int64_t room_id);

    bool AddPlayer(int64_t player_id);
    bool RemovePlayer(int64_t player_id);

    void SetReady(int64_t player_id, bool ready);
    bool CanStart() const;
    void StartGame();

    void OnPlayerDisconnect(int64_t player_id);
    void OnPlayerReconnect(int64_t player_id, SessionPtr session);

    void Broadcast(uint32_t msg_id, const google::protobuf::Message& msg);

    RoomState State() const;
    int64_t RoomId() const;

private:
    int64_t room_id_;
    RoomState state_;
    std::unordered_map<int64_t, RoomPlayer> players_;
};
```

面试重点：

* 房间状态机；
* 玩家断线是否销毁房间；
* 房间什么时候释放；
* 房间里的广播怎么做；
* 多线程访问房间是否需要锁。

---

## 4. `room_manager.h / room_manager.cpp`

功能：

管理所有房间。

```cpp
class RoomManager {
public:
    RoomPtr CreateRoom(const std::vector<int64_t>& player_ids);
    RoomPtr GetRoom(int64_t room_id);
    void RemoveRoom(int64_t room_id);

    RoomPtr GetPlayerRoom(int64_t player_id);

private:
    std::atomic<int64_t> next_room_id_;
    std::unordered_map<int64_t, RoomPtr> rooms_;
    std::unordered_map<int64_t, int64_t> player_room_map_;
};
```

面试重点：

* 如何通过 player_id 找 room；
* 如何通过 room_id 找 room；
* 房间销毁时如何清理 player_room_map；
* room_id 如何生成。

---

# 十三、游戏逻辑模块 `src/game/`

目录：

```text
src/game/
├── game_room.h
├── game_room.cpp
├── game_loop.h
├── game_loop.cpp
├── game_state.h
├── input_buffer.h
├── input_buffer.cpp
├── state_sync.h
├── state_sync.cpp
├── settlement_service.h
└── settlement_service.cpp
```

这个模块是你项目的核心。

---

## 1. `game_state.h`

功能：

定义游戏内状态。

```cpp
struct EntityState {
    int64_t player_id;
    float x;
    float y;
    int32_t hp;
    bool alive;
};

struct GameState {
    int64_t frame_id = 0;
    std::unordered_map<int64_t, EntityState> players;
};
```

---

## 2. `input_buffer.h / input_buffer.cpp`

功能：

缓存玩家输入。

```cpp
struct InputCommand {
    int64_t player_id;
    int64_t input_seq;
    float move_x;
    float move_y;
    bool fire;
    int64_t timestamp_ms;
};

class InputBuffer {
public:
    void Push(const InputCommand& input);
    std::vector<InputCommand> PopAll();

private:
    std::mutex mutex_;
    std::vector<InputCommand> inputs_;
};
```

面试重点：

* 客户端输入不是立刻修改状态，而是进入输入队列；
* 每个 tick 统一处理输入；
* 这样逻辑更稳定，便于同步和回放。

---

## 3. `game_room.h / game_room.cpp`

功能：

真正处理战斗逻辑。

它可以被 `Room` 持有，或者 `Room` 本身继承战斗功能。为了清晰，建议拆出来。

```cpp
class GameRoom {
public:
    explicit GameRoom(int64_t room_id);

    void Start(const std::vector<int64_t>& player_ids);
    void Stop();

    void HandleInput(const InputCommand& input);
    void Tick();

    bool IsGameOver() const;
    int64_t WinnerId() const;

    const GameState& State() const;

private:
    void ApplyInput(const InputCommand& input);
    void CheckGameOver();

private:
    int64_t room_id_;
    GameState state_;
    InputBuffer input_buffer_;
    bool game_over_ = false;
    int64_t winner_id_ = 0;
};
```

Tick 逻辑：

```text
每 50ms 执行一次：
1. 取出所有玩家输入
2. 校验输入合法性
3. 更新玩家位置
4. 处理攻击/碰撞
5. 检查胜负
6. 广播 GameStateNotify
7. 如果结束，进入结算
```

---

## 4. `game_loop.h / game_loop.cpp`

功能：

定时驱动所有正在游戏中的房间。

```cpp
class GameLoop {
public:
    void Start();
    void Stop();

    void AddRoom(std::shared_ptr<GameRoom> room);
    void RemoveRoom(int64_t room_id);

private:
    void Loop();

private:
    std::unordered_map<int64_t, std::shared_ptr<GameRoom>> rooms_;
    std::atomic<bool> running_;
};
```

简单做法：

```text
一个线程每 50ms tick 一次所有游戏房间
```

不要一开始就搞复杂多线程分片。

面试可以说：

> 当前项目为了简化采用单 GameLoop 线程驱动所有房间，后续可以按 room_id hash 分片到多个 GameLoop 线程，减少锁竞争。

这个说法很加分。

---

## 5. `state_sync.h / state_sync.cpp`

功能：

负责把 `GameState` 转成 `GameStateNotify` 协议并广播。

```cpp
class StateSync {
public:
    static proto::GameStateNotify BuildNotify(
        int64_t room_id,
        const GameState& state
    );
};
```

你可以在这里做一些优化：

* 只同步变化状态；
* 限制广播频率；
* 压缩坐标；
* 只给房间内玩家广播。

第一版只做全量广播即可。

---

## 6. `settlement_service.h / settlement_service.cpp`

功能：

处理战斗结算。

```cpp
class SettlementService {
public:
    void SettleBattle(int64_t room_id, int64_t winner_id, const std::vector<int64_t>& losers);

private:
    bool TryInsertSettlementLog(int64_t battle_id, int64_t player_id);
    void UpdatePlayerScore(int64_t player_id, int delta);
};
```

结算流程：

```text
游戏结束
生成 battle_id
插入 settlement_log
更新 MySQL 玩家胜负和积分
更新 Redis 排行榜
广播 GameOverNotify
销毁房间
```

重点是幂等：

```text
settlement_log 设置唯一键 battle_id + player_id
如果重复结算，插入失败，直接跳过
```

面试很喜欢这个点。

---

# 十四、排行榜模块 `src/rank/`

目录：

```text
src/rank/
├── rank_service.h
└── rank_service.cpp
```

---

## `rank_service.h / rank_service.cpp`

功能：

* 更新玩家积分；
* 查询排行榜；
* 使用 Redis Sorted Set；
* 定期或结算时同步 MySQL。

核心类：

```cpp
class RankService {
public:
    bool Init();

    void UpdateScore(int64_t player_id, int score);
    std::vector<RankItem> GetTopN(int offset, int limit);

    void HandleRankRequest(const SessionPtr& session, const Packet& packet);
};
```

Redis 结构：

```text
ZSET rank:score
member = player_id
score = player_score
```

查询：

```text
ZREVRANGE rank:score 0 99 WITHSCORES
```

面试可讲：

* Redis 适合排行榜；
* MySQL 负责持久化；
* Redis 和 MySQL 不一致时如何恢复；
* 结算失败如何重试。

---

# 十五、存储模块 `src/storage/`

目录：

```text
src/storage/
├── mysql_client.h
├── mysql_client.cpp
├── redis_client.h
├── redis_client.cpp
├── player_repository.h
├── player_repository.cpp
├── battle_repository.h
└── battle_repository.cpp
```

---

## 1. `mysql_client.h / mysql_client.cpp`

功能：

封装 MySQL 操作。

```cpp
class MysqlClient {
public:
    bool Connect();
    bool Execute(const std::string& sql);
    QueryResult Query(const std::string& sql);

private:
    // mysql connection
};
```

第一版可以简单一些，不一定要做连接池。
如果要加亮点，可以做：

```text
MysqlConnectionPool
```

但不要一开始就加。

---

## 2. `redis_client.h / redis_client.cpp`

功能：

封装 Redis 操作。

```cpp
class RedisClient {
public:
    bool Connect();

    bool ZAdd(const std::string& key, int score, const std::string& member);
    std::vector<std::pair<std::string, int>> ZRevRange(
        const std::string& key,
        int start,
        int stop
    );

    bool Set(const std::string& key, const std::string& value);
    std::string Get(const std::string& key);
};
```

---

## 3. `player_repository.h / player_repository.cpp`

功能：

玩家数据访问层。

```cpp
class PlayerRepository {
public:
    PlayerData LoadPlayer(int64_t player_id);
    bool CreatePlayer(const PlayerData& data);
    bool UpdatePlayerScore(int64_t player_id, int score_delta);
};
```

不要让业务模块直接写 SQL。
面试可以说：

> 我用 Repository 隔离业务逻辑和数据库访问，方便后续替换存储实现。

---

## 4. `battle_repository.h / battle_repository.cpp`

功能：

战斗记录、结算日志。

```cpp
class BattleRepository {
public:
    bool InsertBattleRecord(const BattleRecord& record);
    bool InsertSettlementLog(const SettlementLog& log);
    bool HasSettlement(int64_t battle_id, int64_t player_id);
};
```

用于：

* 战斗记录；
* 重复结算判断；
* 查询历史战绩。

---

# 十六、定时器模块 `src/timer/`

目录：

```text
src/timer/
├── timer_manager.h
└── timer_manager.cpp
```

---

## `timer_manager.h / timer_manager.cpp`

功能：

处理定时任务。

包括：

* 心跳检查；
* 匹配超时；
* 房间超时；
* 定期落库；
* 定期清理离线 session。

```cpp
class TimerManager {
public:
    void AddTimer(int64_t interval_ms, std::function<void()> callback);
    void Start();
    void Stop();
};
```

第一版可以简化：

```text
一个线程循环检查任务
```

不要上来写复杂时间轮。
可以在 README 里写后续优化：

```text
后续可用时间轮优化大量定时器管理。
```

---

# 十七、工具模块 `src/util/`

目录：

```text
src/util/
├── logger.h
├── logger.cpp
├── time_util.h
├── id_generator.h
├── random_util.h
└── noncopyable.h
```

---

## 1. `logger.h / logger.cpp`

功能：

封装 spdlog。

```cpp
#define LOG_INFO(...)
#define LOG_ERROR(...)
#define LOG_WARN(...)
#define LOG_DEBUG(...)
```

日志要打印：

* 玩家登录；
* 匹配成功；
* 房间创建；
* 游戏开始；
* 结算结果；
* 网络断开；
* 错误消息。

---

## 2. `time_util.h`

功能：

获取毫秒时间戳。

```cpp
int64_t NowMs();
```

用于：

* 心跳；
* 输入时间；
* tick；
* 压测统计。

---

## 3. `id_generator.h`

功能：

生成：

* player_id；
* room_id；
* battle_id；
* session_id。

第一版可以用 atomic：

```cpp
class IdGenerator {
public:
    static int64_t NextRoomId();
    static int64_t NextBattleId();
};
```

面试时可以说：

> 单机版本用 atomic 自增，分布式版本可以改为雪花算法或 Redis INCR。

---

## 4. `random_util.h`

功能：

简单随机数。

用于：

* bot 随机移动；
* 初始出生点；
* 简单匹配测试。

---

# 十八、配置读取模块 `src/config/`

目录：

```text
src/config/
├── config_manager.h
└── config_manager.cpp
```

---

## `config_manager.h / config_manager.cpp`

功能：

读取 `server.yaml`。

```cpp
class ConfigManager {
public:
    static ConfigManager& Instance();

    bool Load(const std::string& path);

    std::string ServerHost() const;
    int ServerPort() const;
    int WorkerThreads() const;
    int TickIntervalMs() const;

    MysqlConfig Mysql() const;
    RedisConfig Redis() const;
};
```

---

# 十九、Bot 压测模块 `bot/`

目录：

```text
bot/
├── CMakeLists.txt
├── bot_main.cpp
├── bot_client.h
├── bot_client.cpp
├── bot_manager.h
└── bot_manager.cpp
```

这个模块非常重要。
有 bot 压测，你的项目就比普通 CRUD 项目强很多。

---

## 1. `bot_main.cpp`

功能：

启动大量 bot。

命令行参数：

```bash
./game_bot --host 127.0.0.1 --port 9000 --count 1000
```

---

## 2. `bot_client.h / bot_client.cpp`

功能：

模拟一个玩家客户端。

流程：

```text
连接服务器
发送 LoginRequest
发送 MatchRequest
收到 MatchSuccessNotify
进入房间
发送 ReadyRequest
游戏中每隔 50ms 发送 InputRequest
收到 GameStateNotify
收到 GameOverNotify
重新匹配
```

核心类：

```cpp
class BotClient {
public:
    void Start();
    void Stop();

private:
    void Login();
    void Match();
    void Ready();
    void SendInput();
    void OnMessage(const Packet& packet);

private:
    int64_t player_id_;
    int64_t room_id_;
};
```

---

## 3. `bot_manager.h / bot_manager.cpp`

功能：

管理多个 bot。

```cpp
class BotManager {
public:
    void Start(int bot_count);
    void Stop();

private:
    std::vector<std::unique_ptr<BotClient>> bots_;
};
```

统计指标：

```text
连接数
登录成功数
匹配成功数
每秒消息数
平均延迟
P99 延迟
断线数
结算次数
```

这些数据可以放进 `docs/pressure_test.md`。

---

# 二十、测试模块 `tests/`

目录：

```text
tests/
├── test_room_state.cpp
├── test_match_queue.cpp
├── test_protocol.cpp
└── test_settlement.cpp
```

---

## 1. `test_room_state.cpp`

测试：

* Waiting -> Ready；
* Ready -> Playing；
* Playing -> Settlement；
* Settlement -> Closed；
* 非法状态切换是否拒绝。

---

## 2. `test_match_queue.cpp`

测试：

* 重复加入匹配；
* 取消匹配；
* 队列满后弹出 N 个玩家；
* 断线时移除匹配队列。

---

## 3. `test_protocol.cpp`

测试：

* Packet 编码；
* Packet 解码；
* 粘包；
* 半包；
* 非法长度包；
* 未知 msg_id。

---

## 4. `test_settlement.cpp`

测试：

* 正常结算；
* 重复结算；
* Redis 更新失败；
* MySQL 更新失败；
* settlement_log 幂等。

---

# 二十一、文档模块 `docs/`

目录：

```text
docs/
├── architecture.md
├── protocol.md
├── pressure_test.md
└── interview_notes.md
```

---

## 1. `architecture.md`

写：

* 总体架构图；
* 消息流转；
* 模块职责；
* 为什么用单进程模块化；
* 后续如何拆分。

建议画：

```text
Client
  |
TcpServer
  |
Codec
  |
MessageDispatcher
  |
Service Layer
  |
Room/Game/Storage
```

---

## 2. `protocol.md`

写：

* 消息头格式；
* msg_id 表；
* 主要 protobuf；
* 登录流程；
* 匹配流程；
* 游戏同步流程。

---

## 3. `pressure_test.md`

写压测结果：

```text
机器配置：
CPU:
内存:
系统:

测试 1：
1000 bot
平均延迟:
P99:
CPU:
内存:

测试 2：
5000 bot
平均延迟:
P99:
CPU:
内存:

瓶颈：
优化：
```

哪怕数据不夸张，也比没有压测强。

---

## 4. `interview_notes.md`

这个是你自己背的，但必须是你理解后的背。

写：

```text
1. TCP 粘包拆包怎么解决？
2. 为什么需要 Session？
3. 玩家断线怎么处理？
4. 匹配队列怎么防重复？
5. 房间状态机怎么设计？
6. 游戏 tick 怎么推进？
7. 状态同步怎么做？
8. 结算怎么保证幂等？
9. Redis 和 MySQL 不一致怎么办？
10. 后续怎么扩展成分布式？
```

---

# 二十二、最小可完成版本

你第一版不要全做。按这个顺序：

## 第一阶段：能连上

```text
main.cpp
GameServer
TcpServer
TcpConnection
Session
Packet
Codec
MessageDispatcher
LoginService
Heartbeat
```

做到：

```text
客户端连接 -> 登录 -> 心跳 -> 断开
```

---

## 第二阶段：能匹配进房间

```text
Player
PlayerManager
MatchService
MatchQueue
Room
RoomManager
RoomState
```

做到：

```text
两个玩家登录 -> 匹配 -> 创建房间 -> 准备 -> 开始
```

---

## 第三阶段：能游戏同步

```text
GameRoom
GameLoop
GameState
InputBuffer
StateSync
```

做到：

```text
客户端发送输入 -> 服务端 tick -> 广播玩家状态
```

---

## 第四阶段：能结算和排行

```text
SettlementService
RankService
MysqlClient
RedisClient
PlayerRepository
BattleRepository
```

做到：

```text
游戏结束 -> 结算积分 -> 写 MySQL -> 更新 Redis 排行榜
```

---

## 第五阶段：能压测

```text
bot_client
bot_manager
pressure_test.md
```

做到：

```text
1000 / 5000 bot 自动登录、匹配、移动、结算
```

---

# 二十三、推荐你最终简历写法

项目名：

**C++ 房间制多人对战游戏服务端**

项目描述：

> 基于 C++17 实现房间制多人对战游戏服务端，包含 TCP 长连接、protobuf 协议、登录会话、匹配队列、房间状态机、固定 tick 状态同步、战斗结算、Redis 排行榜、MySQL 持久化和 bot 压测工具。

项目亮点：

```text
1. 实现 TCP 长连接网络层，支持粘包拆包、心跳检测、连接关闭和消息分发。
2. 设计玩家 Session 与 Player 解耦模型，支持重复登录、断线检测和房间状态恢复。
3. 实现匹配队列和房间状态机，支持玩家匹配、准备、开局、游戏中、结算和房间销毁流程。
4. 使用固定 tick 推进房间逻辑，统一处理玩家输入并广播 GameStateNotify。
5. 使用 MySQL 持久化玩家战绩，Redis Sorted Set 实现实时排行榜。
6. 设计 settlement_log 防止重复结算，保证战斗结算幂等。
7. 编写 bot 压测工具模拟大量玩家登录、匹配、移动和结算，统计延迟、吞吐和资源占用。
```

---

# 二十四、你真正要重点掌握的文件

如果时间有限，下面这些文件必须自己写懂：

```text
src/net/codec.cpp
src/net/session.cpp
src/server/message_dispatcher.cpp
src/login/login_service.cpp
src/match/match_queue.cpp
src/match/match_service.cpp
src/room/room.cpp
src/room/room_manager.cpp
src/game/game_room.cpp
src/game/game_loop.cpp
src/game/settlement_service.cpp
src/rank/rank_service.cpp
bot/bot_client.cpp
```

面试官最可能追问的就是这些。

其他文件可以让 AI 辅助生成，但这些核心文件你必须能从头讲清楚。
