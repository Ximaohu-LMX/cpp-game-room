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

状态是 Waiting -> Ready -> Playing -> Settlement -> Closed。只有所有玩家 ready 才能进入 Ready，只有 Ready 才能 StartGame，结算后关闭房间并清理 player_room_map。

## 6. 游戏 tick 怎么推进？

GameLoop 用一个线程每 50ms tick 所有 GameRoom。玩家输入先进入 InputBuffer，每帧统一消费，避免网络包到达时间直接影响游戏状态。

## 7. 状态同步怎么做？

第一版全量同步 GameStateNotify。后续可做增量同步、坐标压缩、广播频率限制，并且只发给房间内玩家。

## 8. 结算怎么保证幂等？

settlement_log 使用 battle_id + player_id 唯一键。重复结算时插入失败，后续积分更新会跳过，避免重复加减分。

## 9. Redis 和 MySQL 不一致怎么办？

MySQL 是最终持久化来源，Redis 是实时查询加速。恢复时可以从 MySQL 重建排行榜，再根据未处理 settlement_log 或补偿队列追回增量。

## 10. 后续怎么扩展成分布式？

拆出网关服、匹配服、房间服、战斗服和存储服务。网关保持长连接，房间按 room_id hash 路由，结算通过幂等日志和消息队列保证可靠执行。

