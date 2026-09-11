# Interview Notes

## 1. TCP 粘包拆包怎么解决？

TCP 是字节流，没有消息边界。本项目用固定包头：length、msg_id、seq。Codec 先读 length，缓冲区不足就等待更多字节，够一个完整包后再解析；length 超过上限直接断开，避免恶意大包。

## 2. 为什么需要 Session？

TcpConnection 关注网络连接生命周期，Session 关注业务身份。Session 绑定 player_id、room_id 和心跳时间，断线重连时可以恢复玩家在房间内的状态。

## 3. 玩家断线怎么处理？

连接关闭后 ConnectionManager 移除 session。Player 不立刻销毁，Room 内玩家标记为 disconnected，短时间内允许 ReconnectRequest 使用 session_token 恢复。

## 4. 匹配队列怎么防重复？

MatchQueue 同时维护 deque 和 unordered_set。deque 保证匹配顺序，unordered_set 用于 O(1) 判断是否已经在队列中。

## 5. 房间状态机怎么设计？

状态是 Waiting -> Playing -> Settlement -> Closed。所有玩家 ready 的瞬间直接进入 Playing，结算后关闭房间并清理 player_room_map。

## 6. 游戏 tick 怎么推进？

GameLoop 用一个线程每 50ms tick 所有 GameRoom。玩家输入先进入 InputBuffer，每帧统一消费，避免网络包到达时间直接影响游戏状态。

## 7. 状态同步怎么做？

第一版全量同步 GameStateNotify。后续可做增量同步、坐标压缩、广播频率限制，并且只发给房间内玩家。

## 8. 结算怎么保证幂等？

同一局首次结算时由数据库分配 battle_id，后续重试一直复用。事务中先锁定 battle 行，已结算且结果一致时直接复用结果；未结算时，将所有玩家结果、settlement_log、积分与胜负更新一起提交，并设置 settled 标志。复合唯一键提供防重约束，任一步失败则整局回滚。只加唯一键不足以保证日志和积分的一致性。

## 9. Redis 和 MySQL 不一致怎么办？

MySQL 是最终持久化来源。事务提交后读取玩家当前总分，用 ZADD 覆盖 Redis 榜单；Redis 失败时保留房间并复用原 battle_id 重试，MySQL 不再重复加分。当前重试任务仍在内存，尚未实现重启恢复或跨进程版本校验，不能表述为跨存储强一致。

## 10. 后续怎么扩展成分布式？

拆出网关服、匹配服、房间服、战斗服和存储服务。网关保持长连接，房间按 room_id hash 路由，结算通过幂等日志和消息队列保证可靠执行。
