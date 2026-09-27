# 测量与基线复现

S0 接入 #4/#5 的测量版本并固定客户端工具。GameLoop 仍在处理所有房间后 sleep，结算仍同步执行；bot 改为共享事件循环与固定数量工作线程。测得的是**开启这些埋点的版本**，尚未单独测量埋点开销。

## 启用

默认不开启服务端直方图、计时和导出。不依赖 spdlog；JSONL 单独输出。每个开启导出的进程增加一个观察线程。

```bash
cmake -S . -B build-measure -DCMAKE_BUILD_TYPE=Release
cmake --build build-measure -j4

GAME_SERVER_CONFIG=/path/to/test-server.yaml \
GAME_METRICS_FILE=/tmp/server-metrics.jsonl \
./build-measure/src/game_server

./build-measure/bot/game_bot --host 127.0.0.1 --port 9000 \
  --count 100 --duration 80 --metrics-file /tmp/bot-metrics.jsonl \
  --io-threads 4 --seed 20260927 --recovery-file /tmp/recovery.jsonl \
  --account-prefix measurement_ \
  --cancel-match-percent 0 --queue-disconnect-percent 0 \
  --room-disconnect-percent 0 --playing-disconnect-percent 0 \
  --ready-toggle-percent 0
```

`GAME_SERVER_CONFIG` 未设置时仍读取 `config/server.yaml`。输出目录需事先存在；每次启动覆盖指定指标文件，保存历史时使用不同路径。bot 的实时连接和累计事件计数一直存在；RTT 采样随 `--metrics-file` 开启。

## 指标定义

| 字段 | 口径 |
| --- | --- |
| `connections` | ConnectionManager 中的会话数，包括未登录连接；已发生关闭但尚未执行移除回调的会话仍可能短暂计入 |
| `logged_in_players` | player_id 到 Session 映射中的唯一玩家数 |
| `rooms`、`rooms_waiting/playing/settlement/closed` | RoomManager 中的总房间及按状态分类的采样数量，不是通知次数 |
| `match_queue` | 当前匹配队列大小 |
| `loop_work_us` | 整轮快照、所有房间 Tick、状态复制、广播和同步结算回调的总工作时间；不含 sleep |
| `loop_interval_us` | 相邻两轮开始时间差，包含工作和 sleep；实际频率可由其均值或循环计数/时间计算 |
| `room_tick_us` | 单个 GameRoom::Tick，包括输入缓冲取出和锁等待，不含外层广播/结算 |
| `tick_callback_us` | 单房间回调时间，包括状态快照参数求值、广播以及可能发生的结算 |
| `tick_over_budget` | 整轮工作时间超过配置 tick_interval 的次数；不等价于 loop_interval 超过该值 |
| `message_handler_us` | Dispatcher 入口到返回，包括查 handler 和业务处理；不含解码前的网络/排队，也不等到异步发送完成 |
| `login_handler_us` | 登录请求在 Dispatcher 内的同口径耗时 |
| `input_queue_wait_us` | 已找到 GameRoom 的输入，从接收处的单调时间戳到该房间尝试应用输入之前的时间；包括缓冲/锁等待及同批前序输入处理；不含客户端和网络时间 |
| `inputs_received` | 成功交给 GameRoom::HandleInput 的输入数量 |
| `inputs_consumed` | ApplyInput 找到存活玩家后实际应用的输入数量 |
| `inputs_ignored` | 取出后因结束、无此玩家或玩家死亡而忽略的输入；尚未取出的输入不计入 |
| `battle_duration_us` | GameRoom::Start 到首次判定战斗结束；不含匹配、准备和结算重试；仅完成局有样本，未完成局属于未观测完整时长 |
| `settlement_us` | 一次结算尝试，从分配/复用 battle_id 前到该回调返回，成功时包含结束通知投递和房间清理；不是单条 SQL 耗时 |
| `settlement_attempts/failures/success` | 尝试级计数，重试会再次计数；不能把失败尝试数当失败局数 |
| `received_bytes`、`sent_bytes/packets` | socket 成功读取字节、成功写完成字节/包；不含 TCP/IP 头和重传 |
| `read_errors/write_errors` | 错误回调次数，可能包括主动关闭产生的错误，不能当唯一异常断线数 |
| bot `connections_live` | bot 当前认为连通的 socket 数，关闭/读写失败后只扣一次 |
| bot `*_total` | 自进程启动以来的累计事件数；`match_notifications_total`、`playing_notifications_total`、`game_over_notifications_total` 均为每客户端通知数量 |
| bot `input_enqueued_total/input_written_total` | 调用 SendInput 投递的输入数 / async_write 成功完成的输入数，均不代表服务端已应用 |
| bot `unexpected_disconnects_total` | 非主动模拟、非程序退出，由读写/解码失败观察到的连接丢失；网络故障被检测之前不计入 |
| `bot_login_rtt_us` | 成功登录请求从本地投递发送到收到成功响应的时长，包括本地发送排队 |
| `bot_heartbeat_rtt_us` | 心跳从本地投递发送到收到相同包头 seq 的响应的时长；只相减客户端单调时钟 |
| bot 心跳相关计数 | sent、responses、timeouts、cancelled、pending；30 秒以上在下次心跳或响应时检查超时；断开时未完成请求记 cancelled |

心跳响应现在回显请求的**包头 seq**，protobuf 消息定义不变。RTT 采样只能与此版本服务端配合；它不是输入到权威状态的端到端时延。后者仍未测量，因为当前状态通知没有已消费 input_seq 的确认字段。

## JSONL 与分位数

每秒一行：`schema`、`unix_ms`、`elapsed_s`、`interval_s`、`gauges`、`counters`、`histograms`。

- 时长均采用 steady_clock、单位微秒；截断到 1µs，低于 1µs 的样本记为 0。unix_ms 只用于同机样本窗口对齐。
- server counters 累计；bot 带 `_total` 的字段虽然放在 gauges 对象中，也按累计值解释。瞬时量每秒采样，其“最大值”仅为采样峰值。
- histogram 每次导出清空，导出 count、sum_us、max_us、P50/P95/P99 和稀疏桶 `[bucket_index, count]`。固定 256 桶，上界为 `1.1^index µs`。
- 分位数为 nearest-rank 所在桶的上界，并以样本最大值截断；1µs 以上、未溢出时，相对分桶误差不超过 10%。最后一个溢出桶用实际最大值作保守上界。
- count、sum、max 不做分桶近似。空分布不能解释为延迟 0；汇总脚本输出 null。
- 合并报告先合并各窗口桶计数，再求分位数，**不平均每秒的 P99**。
- 不同指标各自同步，不是跨指标的全局原子快照；进程之间导出边界也不完全一致，不能把两个窗口的计数差直接当丢包。
- 导出线程和直方图锁有观察开销；未做关闭埋点的 A/B 实验，不宣称开销可忽略。

## 自动采样

Linux 下使用 Python 标准库和 mysql CLI。需要已启动的**专用测试 MySQL/Redis**，以及有建库权限的测试账号。脚本每轮创建新的 `measure_*` 数据库并初始化仓库表结构，不删除数据库；Redis 使用提供实例中的 `rank:score`，不要指向需要保留排行榜的实例。

```bash
python3 scripts/measure_baseline.py \
  --server ./build-measure/src/game_server \
  --bot ./build-measure/bot/game_bot \
  --server-revision SERVER_COMMIT --bot-revision FROZEN_TOOL_COMMIT \
  --bot-threads 4 --seed 20260927 \
  --config /path/to/test-server.yaml \
  --output /tmp/game-baseline-new \
  --counts 50,100,200,200,200,500 --warmup 20 --seconds 60 \
  --observe-pid mysql=1234 --observe-pid redis=5678
```

output 必须是新的目录。观察 PID 为可选项，填写实际依赖进程 PID。脚本绑定并连接测试配置中的本机服务端端口，依次运行负载；CPU 按“100% 为一个逻辑核”计算，RSS 单位 KiB，同时记录线程和 FD。server/bot 任一提前退出会让本轮失败并保留日志。

每轮保留 server/bot JSONL、两端日志、资源 JSONL、脱敏配置与命令、summary.json；顶层保留机器信息、源码 HEAD、二进制 SHA256 和汇总。采样窗口排除预热，并以窗口内首尾两次完整导出作边界，实际统计时间通常比请求时长少约 1 秒。登录 RTT 主要发生在预热阶段，报告使用全轮分布，须与稳态指标分开。

结束后先停 bot，等待 2 秒观察连接回收，再终止本轮 server；关闭阶段不进入负载窗口。当前业务不保证所有断线房间立即移除，不将残留房间解释为测量器错误。

重新汇总和验证汇总算法：

```bash
python3 scripts/measure_baseline.py --output /tmp/game-baseline-new --summarize-only
python3 -m unittest discover -s scripts -p 'test_measure_baseline.py'
GAME_TEST_CONFIG=/path/to/test-server.yaml ctest --test-dir build-measure --output-on-failure
```

历史工具的结果保留在 [目前的性能测试报告](目前的性能测试报告.md)；S0 新工具的 B0 见 [S0 固定工具与 B0 基线报告](S0固定工具与B0基线报告.md)，不能把两份报告的差值当成服务端优化收益。

## S0 固定工具与可核查口径

`BotManager` 持有一个 `io_context`、work guard 和固定 `--io-threads` 个 worker（默认 4）；地址在启动前解析一次。每个 bot 使用自己的 strand、随机引擎和状态。socket、所有定时器、读写完成、Start/Stop 都经该 strand 串行执行。开启测量后进程另有主线程和 1 个导出线程，因此 500 bot、4 worker 时预期共 6 个线程；无导出时共 5 个。

关闭时先取消定时器和连接，再让事件循环排空回调；读缓冲由连接代次独占，在途写缓冲由完成回调持有。连接/房间/输入循环代次阻止旧回调在重连或下一局继续执行。旧工具可能残留跨局输入定时器，新工具修正这一生命周期问题；这属于工具修正，需要重新建立 B0。

每个 bot 的 `mt19937_64` 以 `seed` 的高低 32 位和 bot index 初始化。相同工具、种子、bot 数量、协议事件顺序可复现每 bot 的随机序列，worker 分配不影响序列。真实多客户端匹配次序、服务器随机逻辑和 OS 调度仍不确定，**种子不保证全局对战逐字节重放**。移动仍为两个 [-1, 1) 随机浮点数、攻击概率 1/4；匹配、准备、断线概率、抖动、结束后再匹配均保留原参数语义。输入仍是每次执行后安排下一次 50 ms 定时器，不补发积压输入，不改成另一个负载模型。

| 新字段 | 定义 |
| --- | --- |
| `io_threads_configured/io_workers_live` | 配置工作线程数 / 当前运行的 worker；另用 /proc 核验进程总线程数 |
| `bot_input_timer_late_us` | 50 ms 输入定时器实际回调开始减 expires_at，反映相对当前发送计划的迟到，不是全局固定频率累计漂移 |
| `bot_input_post_late_us` | SendPacket 投递到 strand 回调开始 |
| `bot_input_write_wait_us` | SendPacket 投递到该输入 async_write 开始，包含 post 和写队列等待 |
| `bot_input_interval_us` | 同一连续输入循环内相邻 SendInput 的间隔；首帧、跨局、跨重连不参与 |
| `input_cancelled_total/input_dropped_total/input_pending` | 关闭时队列中未确认完成 / 投递后因关闭或代次过期丢弃 / 尚待完成的输入 |
| `bot_reconnect_rtt_us` | 发起 ReconnectRequest 到收到成功响应；不含 TCP 重建和重连前等待 |
| `bot_recovery_state_us` | 发起 ReconnectRequest 到响应成功且收到含本玩家的同房间、frame 不倒退的 GameStateNotify；允许状态先到、响应后到 |

`--recovery-file` 逐次记录 bot/index、attempt、player_id、预期/响应 room_id、断线前 frame/hp、观察到的 frame/hp、耗时和 outcome。outcome 为 `state/no_room/rejected/timeout/terminal/invalid/cancelled`；未完成另记 pending。`--recovery-timeout-ms` 默认 30000。等待房间没有 GameState 时可能 timeout；房间已结束返回 no_room；这些均不能当成 Playing 状态恢复成功。此工具采样只验证房间、玩家存在、帧不倒退，hp 留作证据，**未验证 HP 是否被错误重置，也未覆盖 S4 的全部房间状态恢复语义**。

最终导出在所有 worker 排空后执行，可以核查：

- enqueued = written + cancelled + dropped + pending；写完成表示内核接收该次写，不表示服务端业务已应用，取消项也不证明对端未收到部分/全部字节。
- heartbeat_sent = responses + timeouts + cancelled + pending。
- reconnect_attempts = 七种恢复结果计数之和 + recovery_pending。

脚本在这些关系、退出码、pending 清空和线程数验证失败时返回非零，留下原始数据。正常运行中的每秒跨字段快照非原子，不对中间快照要求严格等式。/proc 消失、权限不可读（也可能发生于进程退出过程中）记为缺测；资源汇总保留 samples/unavailable_samples，不伪造零值。

`--server-revision` 和 `--bot-revision` 必填，分别记录 commit、源码 Git 对象与二进制 SHA256；运行前验证相关源码和指定提交一致。服务端身份覆盖 src/proto/config/根 CMake，工具身份覆盖 bot/ 与采样脚本。二进制需由所声明提交构建，脚本不能从任意 ELF 推导其源码来源。先提交工具、构建并测试，再运行基线；之后只提交报告和数据。后续 B1/B2 必须复用这一个 bot 二进制（核对 SHA256）、脚本提交、线程数、种子及命令；工具变化时重新测 B0。

客户端协议回归（Python 标准库，可对 ASan bot 执行同一组测试）：

```bash
GAME_BOT_BINARY=/absolute/path/to/game_bot \
python3 -m unittest discover -s scripts -p 'test_*.py' -v
```

未设置 GAME_BOT_BINARY 时协议测试明确跳过，不能计为通过。测试使用本机受控协议端点，覆盖 500 连接与固定线程、实际输入写入、快速跨局不重复输入循环、种子与 worker 分配无关、异常包关闭、状态先到/后到、无房间、拒绝、超时、终局、错误房间/帧/缺本玩家。它验证测试工具，不代表真实服务端已通过会话正确性验收。
