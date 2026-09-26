# cpp-game-server

一个基于 C++17 的房间制多人对战游戏服务端示例项目，覆盖 TCP 长连接、protobuf 协议、登录会话、匹配队列、房间状态机、固定 tick 状态同步、战斗结算、Redis 排行榜、MySQL 持久化接口和 bot 压测工具。

游戏背景采用回合制单胜者对战：每个房间开启一局 battle，一局游戏只有一个最终胜者，其他参与玩家均为失败方。当前示例实现的是简化战斗流程，用移动、攻击、血量和存活判断模拟战斗，不包含完整商业游戏中的地图、装备、子弹、技能、安全区、回放等复杂玩法。

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
- 简化战斗模型支持玩家移动、范围攻击、血量扣减、死亡判断和唯一胜者判定。
- SettlementService 复用固定 battle_id，通过 MySQL 事务、行锁和复合唯一键实现结算幂等；失败时保留房间重试。
- RankService 使用 Redis Sorted Set 维护排行榜。
- Bot 工具可批量模拟登录、匹配、准备和输入。

## 项目阶段规划

### 一阶段：工程骨架与协议闭环（已完成）

目标：先把服务端基础链路跑通，让客户端请求能从 TCP 连接进入业务层。

已实现：

```text
CMake 工程结构
protobuf 协议定义
TcpServer / TcpConnection
Packet / Codec
MessageDispatcher
ServiceContext
基础日志与配置加载
```

核心能力：

```text
TCP 长连接接入
length + msg_id + seq + protobuf body 拆包
按 msg_id 分发请求
业务 service 通过统一上下文访问依赖
```

### 二阶段：登录、Session 与玩家状态（已完成）

目标：建立玩家身份、连接会话和在线状态之间的关系，为后续匹配和重连做基础。

已实现：

```text
LoginService
Player / PlayerManager
Session
ConnectionManager
Heartbeat
Reconnect
```

核心能力：

```text
登录创建或加载玩家
Session 绑定 player_id / room_id
重复登录时关闭旧连接
心跳更新时间
断线后支持重连恢复玩家状态
```

### 三阶段：匹配队列与房间状态机（已完成）

目标：从单个玩家请求推进到多人房间流程，形成“登录 -> 匹配 -> 建房 -> 准备 -> 开局”的完整链路。

已实现：

```text
MatchQueue
MatchService
Room
RoomManager
RoomState
Ready / Leave 流程
```

核心能力：

```text
deque 保证匹配顺序
unordered_set 防止重复入队
人数满足后创建房间
房间状态 Waiting -> Ready -> Playing -> Settlement -> Closed
Waiting 阶段离开房间后剩余在线玩家可重新入队
```

### 四阶段：固定 Tick 战斗主循环（已完成）

目标：让房间真正进入可推进的战斗状态，并用固定 tick 消费输入、更新状态、广播同步。

已实现：

```text
GameLoop
GameRoom
InputBuffer
StateSync
GameStateNotify
GameOverNotify
```

核心能力：

```text
50ms 固定 tick 推进游戏房间
输入先进入 InputBuffer，再由 GameRoom 统一消费
支持移动、攻击、扣血、死亡和唯一胜者判定
按帧生成房间内玩家状态同步
```

当前取舍：

```text
战斗逻辑采用简化模型，重点验证服务端状态机、输入处理和同步链路。
暂不实现地图、装备、技能、子弹、安全区、回放等复杂玩法。
```

### 五阶段：结算、排行榜与持久化（已完成）

目标：把一局 battle 的结果可靠落库，并维护可查询的玩家积分排行榜。

已实现：

```text
SettlementService
BattleRepository
PlayerRepository
MysqlClient
RedisClient
RankService
mysql.sql
docker-compose mysql / redis
```

核心能力：

```text
battle 记录保存房间和胜者
battle_player_result 保存每名玩家胜负结果和积分变化
固定 battle_id + MySQL 事务 + 复合唯一键，保证重复结算不重复更新积分和胜负
Redis Sorted Set 维护 rank:score 排行榜
MySQL / Redis 均使用真实客户端薄封装
```

### 六阶段：Bot、测试与文档补齐（已完成）

目标：让项目不仅能跑通，还能被验证、压测和讲清楚。

已实现：

```text
game_bot
GoogleTest 单元测试
architecture.md
protocol.md
pressure_test.md
数据库设计.md
interview_notes.md
```

覆盖方向：

```text
协议编解码测试
匹配队列测试
房间状态测试
结算幂等测试
排行榜测试
bot 批量模拟登录、匹配、准备和输入
```

### 优化：抽出原子操作，收敛并发状态切换（已完成）

目标：回看匹配成房和准备开局流程，把容易被并发请求重复触发的状态推进收敛到单个原子操作中。

优化内容：

```text
MatchQueue 增加 TryPopN
匹配成房从 Size + PopN 两步判断，收敛为一次加锁判断并弹出
Room 增加 SetReadyAndTryStart
准备开局从 SetReady + CanStart + StartGame 三步推进，收敛为一次加锁状态推进
```

保留语义：

```text
玩家 ready 仍表示玩家确认准备
RoomState::Ready 仍表示房间内玩家全部确认，已经满足开局条件
优化只改变状态推进的实现方式，避免重复成房或重复启动 GameRoom
```

### 七阶段：压测与可观测性完善（规划中）

目标：用真实数据说明服务端容量和瓶颈，而不是只停留在功能完成。

计划补充：

```text
1000 / 5000 bot 压测记录
平均延迟、P95、P99
QPS、CPU、内存、断线数
房间数、在线人数、tick 耗时
网络收发包统计
结算成功率 / 重试次数
```

输出物：

```text
完善 docs/pressure_test.md
补充关键日志字段
必要时增加 MetricsCollector 或 Prometheus 指标导出
```

### 八阶段：并发模型与性能优化（规划中）

目标：从单进程单主链路扩展到更接近生产形态的多线程房间服务。

计划实现：

```text
GameLoop 按 room_id hash 分片
每个 loop 线程负责一组房间 tick
Session / Room / Player 的并发访问边界梳理
定时器从轮询升级为时间轮
MySQL 连接池
异步结算落库队列
Redis / MySQL 异常时的降级与补偿
```

重点验证：

```text
多房间并发 tick 不互相阻塞
结算逻辑仍保持幂等
断线重连和离开房间在并发场景下状态一致
```

### 九阶段：状态同步与战斗体验增强（规划中）

目标：在不破坏现有框架的基础上，让战斗表现更接近真实多人游戏服务端。

计划实现：

```text
房间广播从全量状态改为增量同步
输入 seq 去重和乱序保护
客户端预测 / 服务端校正预留字段
地图边界与出生点配置
攻击判定参数配置化
观战或战斗回放数据结构预研
```

### 十阶段：服务拆分与高可用演进（远期规划）

目标：在单进程模块化已经清晰的前提下，拆成可独立扩缩容的后端服务。

演进方向：

```text
Gateway 维护 TCP 长连接
LoginService 独立处理账号和会话
MatchService 独立维护匹配池
RoomService 按 room_id hash 分片
BattleService 专注战斗 tick
RankService / StorageService 独立访问 Redis 与 MySQL
```

需要补齐：

```text
内部 RPC 协议
服务注册与发现
跨服务 trace_id
房间迁移或故障恢复策略
排行榜从 MySQL 快照 + settlement_log 恢复
```

## 快速启动

项目只保留真实 MySQL / Redis 存储版本。先安装开发库，例如 Ubuntu / Debian：

```bash
sudo apt-get install -y libmariadb-dev libhiredis-dev
```

启动依赖服务：

```bash
docker compose up -d mysql redis
```

初始化 MySQL 表结构：

```bash
mysql -h127.0.0.1 -P3306 -uroot -p123456 game < config/mysql.sql
```

然后编译并启动：

```bash
cmake -S . -B build
cmake --build build
./build/src/game_server
```

bot 示例：

```bash
./build/bot/game_bot --host 127.0.0.1 --port 9000 --count 1000
```

## 协议设计

网络包头：

```text
| length 4 bytes | msg_id 4 bytes | seq 4 bytes | body |
```

`length` 表示 `msg_id + seq + body` 的长度。body 使用 protobuf 序列化。服务端根据 `msg_id` 分发到对应 service。

## Redis / MySQL 数据设计

- MySQL: `player`、`battle`、`battle_player_result`、`settlement_log`。
- Redis: `ZSET rank:score`，member 为 player_id，score 为玩家积分。Docker Compose 默认映射到宿主机 `6380`，避免和本机已有 Redis 的 `6379` 冲突。

`battle` 表一局游戏一条记录，保存 `battle_id`、`room_id` 和最终 `winner_id`。`battle_player_result` 表一名参与玩家一条记录，保存该玩家在本局中的 `WIN` / `LOSE` 结果和积分变化。`settlement_log` 使用 `(battle_id, player_id)` 唯一键保证同一场战斗同一玩家只结算一次。

当前源码的 MysqlClient / RedisClient 是真实数据库薄封装：MySQL 使用 MySQL/MariaDB C client，Redis 使用 hiredis。

结算改造的事务边界、失败重试、已有数据库升级和测试说明见 [战斗结算幂等](docs/settlement_idempotency.md)。已有数据库必须执行 config/migrations/001_settlement_idempotency.sql 后再运行新代码。

## 压测结果

请在服务器环境填写真实数据，模板见 `docs/pressure_test.md`。建议至少记录 1000 bot 和 5000 bot 的平均延迟、P99、CPU、内存、QPS 和断线数。

## 常见优化

- GameLoop 可按 room_id hash 分片为多个 loop 线程。
- 定时器可从简单轮询升级为时间轮。
- 房间广播可从全量状态改为增量同步。
- MySQL 可增加连接池和异步落库队列。
- Redis 与 MySQL 不一致时，可通过 MySQL 快照恢复排行榜，再消费未落库结算日志。

## 测量与性能基线

已补充可选 JSONL 指标导出、bot 实时连接与 RTT 测量，以及可复现的基线采样脚本。指标口径和运行方法见 [测量说明](docs/measurement.md)，当前实测结果见 [目前的性能测试报告](docs/目前的性能测试报告.md)。
