#pragma once

#include <cstdint>

namespace game {

/**
 * @brief 业务消息 ID。
 * @help 服务端通过 msg_id 将 Packet 分发到不同业务服务。
 */
enum MessageId : uint32_t {
    MSG_LOGIN_REQ = 1001,
    MSG_LOGIN_RESP = 1002,
    MSG_HEARTBEAT_REQ = 1003,
    MSG_HEARTBEAT_RESP = 1004,
    MSG_RECONNECT_REQ = 1005,
    MSG_RECONNECT_RESP = 1006,

    MSG_MATCH_REQ = 2001,
    MSG_MATCH_RESP = 2002,
    MSG_MATCH_SUCCESS_NOTIFY = 2003,
    MSG_MATCH_CANCEL_REQ = 2004,

    MSG_READY_REQ = 3001,
    MSG_READY_NOTIFY = 3002,
    MSG_ENTER_ROOM_REQ = 3003,
    MSG_ENTER_ROOM_RESP = 3004,
    MSG_ROOM_STATE_NOTIFY = 3005,

    MSG_INPUT_REQ = 4001,
    MSG_GAME_STATE_NOTIFY = 4002,
    MSG_GAME_OVER_NOTIFY = 4003,

    MSG_RANK_REQ = 5001,
    MSG_RANK_RESP = 5002,
};

} // namespace game
