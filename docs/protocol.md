# Protocol

## Packet Header

```text
| length 4 bytes | msg_id 4 bytes | seq 4 bytes | protobuf body |
```

`length` 为 `msg_id + seq + body` 的字节数，使用大端编码。`Codec` 根据 length 在 TCP 字节流中拆出完整包，半包会留在缓冲区，非法超大包会直接拒绝。

## Message IDs

```text
1001 MSG_LOGIN_REQ
1002 MSG_LOGIN_RESP
1003 MSG_HEARTBEAT_REQ
1004 MSG_HEARTBEAT_RESP
1005 MSG_RECONNECT_REQ
1006 MSG_RECONNECT_RESP
2001 MSG_MATCH_REQ
2002 MSG_MATCH_RESP
2003 MSG_MATCH_SUCCESS_NOTIFY
2004 MSG_MATCH_CANCEL_REQ
3001 MSG_READY_REQ
3002 MSG_READY_NOTIFY
3003 MSG_ENTER_ROOM_REQ
3004 MSG_ENTER_ROOM_RESP
3005 MSG_ROOM_STATE_NOTIFY
4001 MSG_INPUT_REQ
4002 MSG_GAME_STATE_NOTIFY
4003 MSG_GAME_OVER_NOTIFY
5001 MSG_RANK_REQ
5002 MSG_RANK_RESP
```

## Login Flow

```text
LoginRequest
  -> LoginService parses account/token
  -> get or create player_id
  -> bind Session and Player
  -> close old session if duplicate login
  -> LoginResponse
```

## Match Flow

```text
MatchRequest
  -> MatchQueue.Push(player_id)
  -> when queue size >= room_player_count
  -> RoomManager.CreateRoom
  -> MatchSuccessNotify
```

## Game Sync Flow

```text
InputRequest
  -> RoomManager finds GameRoom
  -> InputBuffer.Push
  -> GameLoop.Tick every 50ms
  -> GameRoom consumes all inputs
  -> StateSync builds GameStateNotify
```

