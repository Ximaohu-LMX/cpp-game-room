# Architecture

## Overall Flow

```text
Client / Bot
  |
  v
TcpServer
  |
  v
TcpConnection
  |
  v
Codec
  |
  v
MessageDispatcher
  |
  +-- LoginService
  +-- MatchService
  +-- RoomManager
  +-- RankService
```

网络层只处理连接生命周期、异步读写和 TCP 字节流拆包。业务层看到的是 Session，它绑定 player_id、room_id 和心跳时间。

## Modules

- `src/net`: TCP server、connection、session、packet、codec。
- `src/protocol`: msg_id 和 protobuf 辅助封装。
- `src/login`: 登录、心跳、重复登录、断线重连。
- `src/match`: 匹配队列和匹配成功通知。
- `src/room`: 房间、房间状态机、准备和输入入口。
- `src/game`: 固定 tick 游戏逻辑、输入缓冲、状态同步、结算。
- `src/storage`: MySQL / Redis 薄封装和 repository。
- `bot`: 压测客户端。

## Why Single Process First

第一版的重点是把链路跑通并能讲清楚：连接、协议、Session、匹配、房间、tick、结算。单进程模块化可以减少 RPC、部署和一致性成本，让问题集中在核心游戏服务端模型上。

## Scale Out

- 网关服维护 TCP 长连接，业务请求转发到后端服务。
- 匹配服独立维护匹配队列。
- 房间服按 room_id hash 分片。
- 战斗服按房间 tick 推进逻辑。
- 排行榜和玩家数据由 Redis / MySQL 提供，结算日志作为幂等和补偿依据。

