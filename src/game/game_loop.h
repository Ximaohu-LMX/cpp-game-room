#pragma once

#include "game/game_room.h"

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>

namespace game {

class GameLoop {
public:
    using TickCallback = std::function<void(int64_t, const GameState&, bool, int64_t)>;

    explicit GameLoop(int tick_interval_ms = 50);
    ~GameLoop();

    void Start();
    void Stop();

    void AddRoom(std::shared_ptr<GameRoom> room);
    void RemoveRoom(int64_t room_id);
    std::shared_ptr<GameRoom> GetRoom(int64_t room_id);
    void SetTickCallback(TickCallback callback);

private:
    void Loop();

    int tick_interval_ms_;
    std::mutex mutex_;
    std::unordered_map<int64_t, std::shared_ptr<GameRoom>> rooms_;
    TickCallback tick_callback_;
    std::atomic<bool> running_{false};
    std::thread worker_;
};

} // namespace game
