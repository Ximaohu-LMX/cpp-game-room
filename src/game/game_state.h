#pragma once

#include <cstdint>
#include <unordered_map>

namespace game {

/**
 * @brief 单个玩家实体在某一帧的状态。
 */
struct EntityState {
    int64_t player_id = 0; ///< 玩家 ID。
    float x = 0;           ///< 当前 x 坐标。
    float y = 0;           ///< 当前 y 坐标。
    int32_t hp = 100;      ///< 当前生命值。
    bool alive = true;     ///< 是否存活。
};

/**
 * @brief 单个 GameRoom 在某一帧的完整状态。
 * @help players 使用 player_id 作为 key，便于根据输入中的 player_id 快速找到实体状态。
 */
struct GameState {
    int64_t frame_id = 0; ///< 游戏帧号，每次 tick 递增。
    std::unordered_map<int64_t, EntityState> players; ///< key 为 player_id，value 为玩家实体状态。
};

} // namespace game
