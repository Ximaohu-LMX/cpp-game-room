#pragma once

#include <cstdint>

namespace game {

/**
 * @brief 房间内玩家状态。
 */
struct RoomPlayer {
    int64_t player_id = 0; ///< 玩家 ID。
    bool ready = false; ///< 是否已准备。
    bool connected = true; ///< 是否保持连接。
    int32_t hp = 100; ///< 当前生命值，当前主要由 GameRoom 使用。
    float x = 0; ///< x 坐标预留字段。
    float y = 0; ///< y 坐标预留字段。
};

} // namespace game
