# 战斗结算幂等

同一局战斗重复调用结算接口时，MySQL 中的玩家积分和胜负次数只更新一次。数据库写入失败后，可以复用原来的 battle_id 重试；Redis 刷新失败也不会让积分再次增加。

## 原实现的问题

- SettleBattle 每次生成新的 battle_id，重试会绕过复合唯一键。
- 结算日志和玩家积分分开提交，日志成功而积分失败时会出现部分结算。
- INSERT IGNORE 把重复和部分 SQL 错误混在一起，调用方没有检查整局是否成功。
- 结算失败也会关闭房间，无法继续重试。
- 进程内 ID 计数器在重启后重置，可能撞上历史战斗或结算日志的主键。

## 当前流程

1. 战斗首次进入结算时，GameServer 调用 BattleRepository::CreateBattle，由 MySQL 自增主键分配 battle_id。此时只预留记录，winner_id 为 0、settled 为 0。同一房间的后续结算尝试复用这个 ID。
2. SettlementService 校验参与者，将玩家排序、去重，再调用 BattleRepository::ApplySettlement。
3. MysqlClient::RunTransaction 开启事务，并在事务整个生命周期持有连接锁，防止同一连接上的其他业务 SQL 混入。
4. 对 battle 记录执行 SELECT ... FOR UPDATE。不同数据库连接对同一局的并发结算在这里串行化。
5. 如果 settled 已为 1，核对房间、胜者以及所有玩家结果。结果一致则按成功重试处理；结果冲突则返回失败。
6. 如果尚未结算，在同一事务中插入每位玩家的结果、结算日志，更新积分及胜负统计，最后设置 battle.settled = 1 和 end_time。任一步失败都回滚。
7. MySQL 提交后，从 player 表读取当前总分并用 Redis ZADD 覆盖榜单分数。命中重复结算时仍执行这一刷新，以补偿上一次 Redis 写入失败。
8. MySQL 或 Redis 操作失败时，GameServer 保留 Settlement 状态和 battle_id，每秒重试一次。全部成功后才发送 GameOverNotify 并关闭房间。

settlement_log 和 battle_player_result 都保留 (battle_id, player_id) 唯一约束；settlement_log 的主键也由数据库自增分配。

## 失败和重试的结果

| 情况 | 处理 |
| --- | --- |
| 相同 battle_id 和相同战斗结果重复提交 | 返回成功，积分与胜负次数不再更新 |
| 多个数据库连接并发结算同一局 | battle 行锁串行化，只提交一次 |
| 同一 battle_id 携带不同胜者或参与者 | 拒绝，不修改已有结果 |
| 更新后面的玩家时 SQL 失败 | 回滚此前玩家的积分、战绩和结算日志 |
| COMMIT 响应丢失 | 关闭连接并返回失败；复用原 ID 重试时查询已提交标志 |
| MySQL 成功、Redis 失败 | 保留重试；只刷新榜单，不重复加减分 |
| 旧战斗在新战斗结束后重试 | 从 MySQL 读取最新总分，避免把旧局分数重新写回排行榜 |
| 结算期间收到 ready 请求 | 保持 Settlement，禁止重新启动战斗 |

## 数据库升级

新建数据库执行 config/mysql.sql。已有数据库先停止旧版本服务，再执行一次：

```bash
mysql -h127.0.0.1 -P3306 -uroot -p game < config/migrations/001_settlement_idempotency.sql
```

迁移将四张表明确设为 InnoDB，为 battle 增加 settled 字段，并将 battle_id 和 settlement_id 改为自增主键。新代码依赖这些字段，CREATE TABLE IF NOT EXISTS 不会升级已有表。迁移不会自动修复旧版本遗留的部分结算数据。

## 测试

```bash
cmake -S . -B build-idempotency -DBUILD_TESTS=ON
cmake --build build-idempotency -j4
GAME_TEST_CONFIG=/path/to/test-server.yaml ctest --test-dir build-idempotency --output-on-failure
```

GAME_TEST_CONFIG 可指定独立 MySQL/Redis 实例，未设置时使用 config/server.yaml。结算测试创建独立 ID 的数据，并在结束后删除自己的记录。数据库不可用时集成测试会跳过，因此验证时需要确认这些用例实际执行。

test_settlement.cpp 覆盖复合唯一键、顺序重复、不同连接并发、SQL 中途失败后的整局回滚与重试、冲突结果、新战斗 ID、数据库断连后的 ID 分配重试、事务异常回滚、榜单失败后的重试，以及旧请求使用最新积分刷新榜单。test_room_state.cpp 验证结算期间无法重新准备开局。

## 本次验证记录

- Debug 构建通过，服务端、bot 和测试程序均完成编译。
- 在独立 MySQL 8.4 / Redis 7 实例上执行 19 项测试，全部通过，0 项跳过。
- 从旧表结构执行迁移后，历史记录保留；新 battle_id 和 settlement_id 从已有最大值继续分配。
- 10 个 bot 的 Redis 中断/恢复验证共完成 13 局、收到 26 次 GameOver 通知；玩家积分与胜负统计和结算日志一致。Redis 不可用时已有 MySQL 结算提交，恢复后主流程自动继续。
- 上述验证使用 /tmp 中的独立数据库和构建目录，没有升级项目原有运行数据库。

## 实现边界

- MySQL 中的结算是事务性、幂等的；MySQL 与 Redis 不属于同一个事务，榜单在失败重试完成前可能滞后。
- 当前服务只有一个 SettlementService，通过其互斥锁串行化结算与榜单刷新。跨进程部署需要额外的积分版本校验或补偿消费者，才能避免多个写入者之间的旧值覆盖。
- 待重试房间和未提交的战斗结果仍在内存中，尚未实现进程崩溃后的自动恢复。恢复任务若重新提交一份已知战斗结果，必须携带原 battle_id。
- 战斗 ID 不再依赖进程内计数器；玩家和房间 ID 的生成方式保持原状。
- 结算仍同步执行于 GameLoop 回调，数据库或 Redis 的慢请求仍可能拖慢 tick。

## 简历表述

通过固定 battle_id 与玩家 ID 的复合唯一约束实现结算防重，将战绩、结算日志及玩家积分/胜负统计纳入 MySQL 事务，支持重复结算与失败重试；提交后读取最终积分更新 Redis 排行榜。
