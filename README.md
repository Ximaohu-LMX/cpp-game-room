# cpp-game-server

一个基于 C++17 的房间制多人对战游戏服务端示例项目，覆盖 TCP 长连接、protobuf 协议、登录会话、匹配队列、房间状态机、固定 tick 状态同步、战斗结算、Redis 排行榜、MySQL 持久化接口和 bot 压测工具。

## 技术栈

- C++17
- Boost.Asio
- protobuf
- spdlog
- Redis / MySQL
- CMake
- Docker Compose
- GoogleTest

## 架构设计

```text
Client / Bot
   |
TcpServer / TcpConnection
   |
Codec: length + msg_id + seq + protobuf body
   |
MessageDispatcher
   |
+-- LoginService
+-- MatchService
+-- RoomManager
+-- GameLoop / GameRoom
+-- RankService
+-- Repository / Storage
```

第一版保持单进程模块化：网络层只负责连接和收发包，Session 承载玩家身份和房间状态，业务服务通过 ServiceContext 注入依赖。后续可以按模块拆成网关、匹配服、房间服、战斗服和存储服务。

## 核心功能

- TCP 长连接，支持粘包拆包、心跳、关闭和消息分发。
- Session 与 Player 解耦，支持重复登录、断线和重连状态恢复。
- MatchQueue 使用 deque + unordered_set，保证有序匹配并防止重复入队。
- Room 状态机：Waiting -> Ready -> Playing -> Settlement -> Closed。
- GameLoop 以固定 50ms tick 推进 GameRoom，统一消费玩家输入并同步状态。
- SettlementService 通过 settlement_log 思路保证战斗结算幂等。
- RankService 支持 Redis Sorted Set 排行榜；默认内存实现方便本地演示，开启 `GAME_USE_REDIS` 后接入真实 Redis。
- Bot 工具可批量模拟登录、匹配、准备和输入。

## 快速启动

默认构建使用内存 storage，便于不启动外部依赖时跑通单元测试和主流程：

```bash
mkdir build
cd build
cmake ..
cmake --build .
./game_server
```

真实 MySQL / Redis 模式需要先安装开发库，例如 Ubuntu / Debian：

```bash
sudo apt-get install -y libmariadb-dev libhiredis-dev
```

启动依赖服务：

```bash
docker compose up -d mysql redis
```

然后开启真实 storage 编译：

```bash
cmake -S . -B build-real -DGAME_USE_MYSQL=ON -DGAME_USE_REDIS=ON
cmake --build build-real
./build-real/src/game_server
```

bot 示例：

```bash
./build-real/bot/game_bot --host 127.0.0.1 --port 9000 --count 1000
```

## 协议设计

网络包头：

```text
| length 4 bytes | msg_id 4 bytes | seq 4 bytes | body |
```

`length` 表示 `msg_id + seq + body` 的长度。body 使用 protobuf 序列化。服务端根据 `msg_id` 分发到对应 service。

## Redis / MySQL 数据设计

- MySQL: `player`、`battle_record`、`settlement_log`。
- Redis: `ZSET rank:score`，member 为 player_id，score 为玩家积分。Docker Compose 默认映射到宿主机 `6380`，避免和本机已有 Redis 的 `6379` 冲突。

`settlement_log` 使用 `(battle_id, player_id)` 唯一键保证同一场战斗同一玩家只结算一次。`battle_record` 使用 `(battle_id, loser_id)` 唯一键，支持多人房间下一场战斗产生多条战绩记录。

当前源码的 MysqlClient / RedisClient 是可替换的薄封装：默认构建使用内存结构；开启 `GAME_USE_MYSQL` / `GAME_USE_REDIS` 后分别使用 MySQL/MariaDB C client 和 hiredis 连接真实服务。

## 压测结果

请在服务器环境填写真实数据，模板见 `docs/pressure_test.md`。建议至少记录 1000 bot 和 5000 bot 的平均延迟、P99、CPU、内存、QPS 和断线数。

## 常见优化

- GameLoop 可按 room_id hash 分片为多个 loop 线程。
- 定时器可从简单轮询升级为时间轮。
- 房间广播可从全量状态改为增量同步。
- MySQL 可增加连接池和异步落库队列。
- Redis 与 MySQL 不一致时，可通过 MySQL 快照恢复排行榜，再消费未落库结算日志。
