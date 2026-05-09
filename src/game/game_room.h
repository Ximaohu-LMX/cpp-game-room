#pragma once

#include "game/game_state.h"
#include "game/input_buffer.h"

#include <cstdint>
#include <mutex>
#include <vector>

namespace game {

class GameRoom {
public:
    explicit GameRoom(int64_t room_id);

    int64_t RoomId() const;
    void Start(const std::vector<int64_t>& player_ids);
    void Stop();

    void HandleInput(const InputCommand& input);
    void Tick();

    bool IsGameOver() const;
    int64_t WinnerId() const;

    GameState State() const;

private:
    void ApplyInput(const InputCommand& input);
    void CheckGameOver();

    int64_t room_id_;
    mutable std::mutex mutex_;
    GameState state_;
    InputBuffer input_buffer_;
    bool game_over_ = false;
    int64_t winner_id_ = 0;
};

} // namespace game

