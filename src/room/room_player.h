#pragma once

#include <cstdint>

namespace game {

struct RoomPlayer {
    int64_t player_id = 0;
    bool ready = false;
    bool connected = true;
    int32_t hp = 100;
    float x = 0;
    float y = 0;
};

} // namespace game

