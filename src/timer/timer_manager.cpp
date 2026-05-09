#include "timer/timer_manager.h"

#include <chrono>

namespace game {

TimerManager::~TimerManager() {
    Stop();
}

void TimerManager::AddTimer(int64_t interval_ms, std::function<void()> callback) {
    TimerTask task;
    task.interval_ms = interval_ms;
    task.callback = std::move(callback);
    task.next_run = std::chrono::steady_clock::now() + std::chrono::milliseconds(interval_ms);

    std::lock_guard<std::mutex> lock(mutex_);
    tasks_.push_back(std::move(task));
}

void TimerManager::Start() {
    if (running_.exchange(true)) {
        return;
    }
    worker_ = std::thread([this]() { Loop(); });
}

void TimerManager::Stop() {
    if (!running_.exchange(false)) {
        return;
    }
    if (worker_.joinable()) {
        worker_.join();
    }
}

void TimerManager::Loop() {
    while (running_) {
        const auto now = std::chrono::steady_clock::now();
        std::vector<std::function<void()>> callbacks;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            for (auto& task : tasks_) {
                if (now >= task.next_run) {
                    callbacks.push_back(task.callback);
                    task.next_run = now + std::chrono::milliseconds(task.interval_ms);
                }
            }
        }
        for (auto& callback : callbacks) {
            callback();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

} // namespace game

