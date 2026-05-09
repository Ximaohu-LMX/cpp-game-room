#pragma once

#include <cstdint>
#include <unordered_map>

namespace game {

struct EntityState {
    int64_t player_id = 0;
    float x = 0;
    float y = 0;
    int32_t hp = 100;
    bool alive = true;
};

struct GameState {
    int64_t frame_id = 0;
    std::unordered_map<int64_t, EntityState> players;
};

} // namespace game

