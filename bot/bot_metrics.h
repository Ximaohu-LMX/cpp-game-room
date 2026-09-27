#pragma once
#include "util/metrics.h"
#include <fstream>
#include <memory>

namespace game {
struct BotStats {
    std::atomic<int64_t> connections_live{0};
    std::atomic<int64_t> unexpected_disconnects{0};
    std::atomic<int64_t> packets_written{0};
    std::atomic<int64_t> input_written{0};
    std::atomic<int64_t> login_attempts{0};
    std::atomic<int64_t> heartbeat_sent{0};
    std::atomic<int64_t> heartbeat_responses{0};
    std::atomic<int64_t> heartbeat_timeouts{0};
    std::atomic<int64_t> heartbeat_cancelled{0};
    std::atomic<int64_t> heartbeat_pending{0};
    std::atomic<int64_t> connect_ok{0};
    std::atomic<int64_t> connect_failed{0};
    std::atomic<int64_t> disconnects{0};
    std::atomic<int64_t> login_ok{0};
    std::atomic<int64_t> login_failed{0};
    std::atomic<int64_t> reconnect_ok{0};
    std::atomic<int64_t> reconnect_failed{0};
    std::atomic<int64_t> match_ok{0};
    std::atomic<int64_t> match_failed{0};
    std::atomic<int64_t> match_cancel{0};
    std::atomic<int64_t> match_success{0};
    std::atomic<int64_t> ready{0};
    std::atomic<int64_t> unready{0};
    std::atomic<int64_t> room_playing{0};
    std::atomic<int64_t> input_sent{0};
    std::atomic<int64_t> game_state{0};
    std::atomic<int64_t> game_over{0};
    std::atomic<int64_t> input_cancelled{0}, input_dropped{0}, input_pending{0};
    std::atomic<int64_t> reconnect_attempts{0}, recovery_state{0}, recovery_no_room{0};
    std::atomic<int64_t> recovery_rejected{0}, recovery_timeout{0}, recovery_cancelled{0};
    std::atomic<int64_t> recovery_terminal{0}, recovery_invalid{0}, recovery_pending{0};
    std::atomic<int64_t> workers_live{0}, worker_exceptions{0};
    std::atomic<bool> measurements{false};
    Histogram login_rtt, heartbeat_rtt, input_timer_late, input_post_late;
    Histogram input_write_wait, input_interval, reconnect_rtt, recovery_latency;
    void Observe(Histogram& histogram, uint64_t us) {
        if (measurements.load(std::memory_order_relaxed)) histogram.Observe(us);
    }
    bool OpenRecoveryLog(const std::string& path);
    void RecoveryEvent(const std::string& json);
private:
    std::mutex event_mutex_;
    std::ofstream recovery_log_;
};

class BotMetricsReporter {
public:
    ~BotMetricsReporter() { Stop(); }
    bool Start(const std::string& path, std::shared_ptr<BotStats> stats,
               std::function<std::map<std::string, int64_t>()> gauges);
    void Stop();
private:
    std::ofstream output_;
    std::thread worker_;
    std::mutex mutex_;
    std::condition_variable wake_;
    bool stopped_ = true;
    std::shared_ptr<BotStats> stats_;
};
} // namespace game
