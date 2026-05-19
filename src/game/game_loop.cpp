#include "game/game_loop.h"

#include <chrono>
#include <utility>
#include <vector>

namespace game {

// tick_interval_ms 是固定帧间隔，配置非法时回退到 50ms。
GameLoop::GameLoop(int tick_interval_ms) : tick_interval_ms_(tick_interval_ms > 0 ? tick_interval_ms : 50) {}

GameLoop::~GameLoop() {
    Stop();
}

void GameLoop::Start() {
    // exchange(true) 可以避免重复启动多个 GameLoop 线程。
    if (running_.exchange(true)) {
        return;
    }
    // GameLoop 独立线程负责按固定 tick 推进所有战斗房间。
    worker_ = std::thread([this]() { Loop(); });
}

void GameLoop::Stop() {
    // exchange(false) 可以避免重复停止；停止后等待 worker 线程退出。
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
    // rooms_ 是 GameLoop 管理的运行中战斗房间表，需要加锁保护。
    std::lock_guard<std::mutex> lock(mutex_);
    rooms_[room->RoomId()] = std::move(room);
}

void GameLoop::RemoveRoom(int64_t room_id) {
    // 房间结算关闭后从 GameLoop 移除，后续 tick 不再推进它。
    std::lock_guard<std::mutex> lock(mutex_);
    rooms_.erase(room_id);
}

std::shared_ptr<GameRoom> GameLoop::GetRoom(int64_t room_id) {
    // 输入请求到达时，RoomManager 会通过 room_id 找到对应 GameRoom 并写入输入缓冲。
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = rooms_.find(room_id);
    return it == rooms_.end() ? nullptr : it->second;
}

void GameLoop::SetTickCallback(TickCallback callback) {
    // tick 回调用于把 GameRoom 的状态交给外层做广播、结算和房间关闭。
    std::lock_guard<std::mutex> lock(mutex_);
    tick_callback_ = std::move(callback);
}

void GameLoop::Loop() {
    while (running_) {
        std::vector<std::shared_ptr<GameRoom>> rooms;
        TickCallback callback;
        {
            // 只在这里短暂持锁，拷贝房间 shared_ptr 和回调。
            // 真正 Tick 房间时不持有 mutex，避免长时间阻塞 AddRoom/RemoveRoom/GetRoom。
            // lock_guard 利用 RAII（对象出作用域自动析构）保证无论怎么退出都会解锁。
            std::lock_guard<std::mutex> lock(mutex_);
            for (const auto& [_, room] : rooms_) {
                rooms.push_back(room);
            }
            callback = tick_callback_;
        }

        for (auto& room : rooms) {
            // GameRoom::Tick 会消费输入、推进玩家位置/血量/存活状态，并判断是否结束。
            room->Tick();
            if (callback) {
                // GameLoop 不直接做网络广播和结算，只把当前房间状态交给回调处理。
                callback(room->RoomId(), room->State(), room->IsGameOver(), room->WinnerId());
            }
        }

        // 简化实现：每轮处理完所有房间后 sleep 固定间隔。
        // 生产环境可改为基于 steady_clock 的下一帧时间，减少 tick 漂移。
        std::this_thread::sleep_for(std::chrono::milliseconds(tick_interval_ms_));
    }
}

} // namespace game
