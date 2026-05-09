# Pressure Test

## Environment

```text
CPU:
Memory:
OS:
Compiler:
Network:
```

## Test 1: 1000 Bots

```text
Average latency:
P99 latency:
QPS:
CPU:
Memory:
Disconnects:
Settlement count:
```

## Test 2: 5000 Bots

```text
Average latency:
P99 latency:
QPS:
CPU:
Memory:
Disconnects:
Settlement count:
```

## Bottlenecks

- Single GameLoop thread will become hot when room count grows.
- Full state broadcast increases bandwidth.
- Storage writes should be batched or async in higher pressure tests.

## Optimization Plan

- Split GameLoop by room_id hash.
- Use delta state sync.
- Add async settlement queue.
- Add MySQL connection pool.
- Use time wheel for massive heartbeat and timeout tasks.

