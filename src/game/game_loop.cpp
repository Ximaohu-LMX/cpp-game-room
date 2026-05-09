#include "game/game_loop.h"

#include <chrono>
#include <utility>
#include <vector>

namespace game {

GameLoop::GameLoop(int tick_interval_ms) : tick_interval_ms_(tick_interval_ms > 0 ? tick_interval_ms : 50) {}

GameLoop::~GameLoop() {
    Stop();
}

void GameLoop::Start() {
    if (running_.exchange(true)) {
        return;
    }
    worker_ = std::thread([this]() { Loop(); });
}

void GameLoop::Stop() {
    if (!running_.exchange(false)) {
        return;
    }
    if (worker_.joinable()) {
        worker_.join();
    }
}

void GameLoop::AddRoom(std::shared_ptr<GameRoom> room) {
    if (!room) {
        return;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    rooms_[room->RoomId()] = std::move(room);
}

void GameLoop::RemoveRoom(int64_t room_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    rooms_.erase(room_id);
}

std::shared_ptr<GameRoom> GameLoop::GetRoom(int64_t room_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = rooms_.find(room_id);
    return it == rooms_.end() ? nullptr : it->second;
}

void GameLoop::SetTickCallback(TickCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    tick_callback_ = std::move(callback);
}

void GameLoop::Loop() {
    while (running_) {
        std::vector<std::shared_ptr<GameRoom>> rooms;
        TickCallback callback;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            for (const auto& [_, room] : rooms_) {
                rooms.push_back(room);
            }
            callback = tick_callback_;
        }

        for (auto& room : rooms) {
            room->Tick();
            if (callback) {
                callback(room->RoomId(), room->State(), room->IsGameOver(), room->WinnerId());
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(tick_interval_ms_));
    }
}

} // namespace game
