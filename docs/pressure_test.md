# Pressure Test

## Bot Flow

`game_bot` now runs a mixed state-flow pressure test instead of only the happy path.

```text
login
  -> match
     -> optional cancel match
     -> optional disconnect while still in MatchQueue
  -> match success / room waiting
     -> optional disconnect before ready
     -> ready
     -> optional unready
     -> ready again
  -> room playing
     -> optional disconnect during battle
     -> reconnect
     -> input loop
  -> game over
  -> rematch
```

This covers the important player/room transitions:

```text
Online -> Matching -> Online
Online -> Matching -> InRoom -> Playing
InRoom/Playing -> disconnected socket -> ReconnectRequest -> continue
Playing -> Settlement/Closed -> next match
```

## Commands

Small local smoke test:

```bash
./build/bot/game_bot --host 127.0.0.1 --port 9000 --count 20 --duration 60 --verbose
```

Mixed pressure test:

```bash
./build/bot/game_bot \
  --host 127.0.0.1 \
  --port 9000 \
  --count 1000 \
  --duration 300 \
  --cancel-match-percent 10 \
  --queue-disconnect-percent 5 \
  --room-disconnect-percent 5 \
  --playing-disconnect-percent 5 \
  --ready-toggle-percent 10 \
  --max-reconnects 2
```

Focused cancellation test:

```bash
./build/bot/game_bot \
  --host 127.0.0.1 \
  --port 9000 \
  --count 200 \
  --duration 120 \
  --cancel-match-percent 50 \
  --queue-disconnect-percent 0 \
  --room-disconnect-percent 0 \
  --playing-disconnect-percent 0
```

Focused reconnect test:

```bash
./build/bot/game_bot \
  --host 127.0.0.1 \
  --port 9000 \
  --count 200 \
  --duration 120 \
  --cancel-match-percent 0 \
  --queue-disconnect-percent 20 \
  --room-disconnect-percent 20 \
  --playing-disconnect-percent 20 \
  --max-reconnects 3
```

Watch these counters while running:

```text
cancel          match cancel requests sent
disconnect      client-side simulated disconnects
reconnect       successful ReconnectResponse
rooms           MatchSuccessNotify count
ready/unready   ready toggle coverage
playing         RoomStateNotify(Playing) count
over            GameOverNotify count
```

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
