#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

namespace game {

/**
 * @brief 简单周期定时器管理器。
 * @help 独立线程轮询任务，当前用于心跳超时检查。
 */
class TimerManager {
public:
    /**
     * @brief 析构时停止定时器线程。
     */
    ~TimerManager();

    /**
     * @brief 添加周期任务。
     * @param interval_ms 执行间隔，单位毫秒。
     * @param callback 到期执行的回调。
     */
    void AddTimer(int64_t interval_ms, std::function<void()> callback);

    /**
     * @brief 启动定时器线程。
     */
    void Start();

    /**
     * @brief 停止定时器线程。
     */
    void Stop();

private:
    /**
     * @brief 定时任务。
     */
    struct TimerTask {
        int64_t interval_ms = 0; ///< 执行间隔。
        std::function<void()> callback; ///< 任务回调。
        std::chrono::steady_clock::time_point next_run; ///< 下次执行时间。
    };

    /**
     * @brief 定时器主循环。
     */
    void Loop();

    std::mutex mutex_;
    std::vector<TimerTask> tasks_;
    std::atomic<bool> running_{false};
    std::thread worker_;
};

} // namespace game
