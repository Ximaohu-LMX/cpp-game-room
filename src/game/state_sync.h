#pragma once

#include "game/game_state.h"

#include "game.pb.h"

namespace game {

class StateSync {
public:
    static proto::GameStateNotify BuildNotify(int64_t room_id, const GameState& state);
};

} // namespace game

