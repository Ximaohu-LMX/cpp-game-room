#include "game/game_room.h"

#include <algorithm>
#include <cmath>

namespace game {

GameRoom::GameRoom(int64_t room_id) : room_id_(room_id) {}

int64_t GameRoom::RoomId() const {
    return room_id_;
}

void GameRoom::Start(const std::vector<int64_t>& player_ids) {
    std::lock_guard<std::mutex> lock(mutex_);
    state_.frame_id = 0;
    state_.players.clear();
    float spawn_x = 0.0f;
    for (auto player_id : player_ids) {
        EntityState entity;
        entity.player_id = player_id;
        entity.x = spawn_x;
        entity.y = 0.0f;
        entity.hp = 100;
        entity.alive = true;
        state_.players[player_id] = entity;
        spawn_x += 5.0f;
    }
    game_over_ = false;
    winner_id_ = 0;
}

void GameRoom::Stop() {
    std::lock_guard<std::mutex> lock(mutex_);
    game_over_ = true;
}

void GameRoom::HandleInput(const InputCommand& input) {
    input_buffer_.Push(input);
}

void GameRoom::Tick() {
    auto inputs = input_buffer_.PopAll();
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (game_over_) {
            return;
        }

        ++state_.frame_id;
        for (const auto& input : inputs) {
            ApplyInput(input);
        }
        CheckGameOver();
    }
}

bool GameRoom::IsGameOver() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return game_over_;
}

int64_t GameRoom::WinnerId() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return winner_id_;
}

GameState GameRoom::State() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return state_;
}

void GameRoom::ApplyInput(const InputCommand& input) {
    auto it = state_.players.find(input.player_id);
    if (it == state_.players.end() || !it->second.alive) {
        return;
    }

    const float max_axis = 1.0f;
    const float dx = std::clamp(input.move_x, -max_axis, max_axis);
    const float dy = std::clamp(input.move_y, -max_axis, max_axis);
    it->second.x += dx * 0.6f;
    it->second.y += dy * 0.6f;

    if (!input.fire) {
        return;
    }

    for (auto& [target_id, target] : state_.players) {
        if (target_id == input.player_id || !target.alive) {
            continue;
        }
        const float distance = std::hypot(target.x - it->second.x, target.y - it->second.y);
        if (distance <= 8.0f) {
            target.hp -= 10;
            if (target.hp <= 0) {
                target.hp = 0;
                target.alive = false;
            }
        }
    }
}

void GameRoom::CheckGameOver() {
    int alive_count = 0;
    int64_t alive_player = 0;
    for (const auto& [player_id, entity] : state_.players) {
        if (entity.alive) {
            ++alive_count;
            alive_player = player_id;
        }
    }
    if (alive_count <= 1 && !state_.players.empty()) {
        game_over_ = true;
        winner_id_ = alive_player;
    }
}

} // namespace game

