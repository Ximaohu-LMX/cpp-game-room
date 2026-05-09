#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

namespace game {

class TimerManager {
public:
    ~TimerManager();

    void AddTimer(int64_t interval_ms, std::function<void()> callback);
    void Start();
    void Stop();

private:
    struct TimerTask {
        int64_t interval_ms = 0;
        std::function<void()> callback;
        std::chrono::steady_clock::time_point next_run;
    };

    void Loop();

    std::mutex mutex_;
    std::vector<TimerTask> tasks_;
    std::atomic<bool> running_{false};
    std::thread worker_;
};

} // namespace game
