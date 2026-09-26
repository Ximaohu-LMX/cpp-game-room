#pragma once

#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <fstream>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <thread>

namespace game {

enum class Counter {
    ConnectionsAccepted, ConnectionsRemoved, MessagesReceived, UnknownMessages,
    ReadErrors, WriteErrors, ReceivedBytes, SentBytes, SentPackets,
    InputsReceived, InputsConsumed, InputsIgnored, RoomsCreated, RoomsRemoved,
    BattlesStarted, BattlesFinished, SettlementAttempts, SettlementFailures,
    SettlementSuccess, LoopIterations, TickOverBudget, Count
};
enum class Distribution {
    LoopWork, LoopInterval, RoomTick, TickCallback, MessageHandler, LoginHandler,
    InputQueueWait, BattleDuration, Settlement, BotLoginRtt, BotHeartbeatRtt, Count
};

// Fixed memory; percentiles are bucket upper bounds (<=10% error above 1 us).
// Exported buckets can be merged across intervals without averaging percentiles.
struct HistogramSnapshot {
    static constexpr size_t kBuckets = 256;
    std::array<uint64_t, kBuckets> buckets{};
    uint64_t count = 0, sum_us = 0, max_us = 0;
    double Percentile(double quantile) const;
    static double UpperBound(size_t index);
};
class Histogram {
public:
    void Observe(uint64_t microseconds);
    HistogramSnapshot Drain();
private:
    std::mutex mutex_;
    HistogramSnapshot data_;
};

// Disabled by default. Counters are cumulative; histograms reset on export.
class Metrics {
public:
    static Metrics& Instance();
    bool Enabled() const { return enabled_.load(std::memory_order_relaxed); }
    void Enable(bool enabled) { enabled_.store(enabled, std::memory_order_relaxed); }
    void Increment(Counter metric, uint64_t amount = 1);
    void Observe(Distribution metric, uint64_t microseconds);
    std::string SnapshotJson(const std::map<std::string, int64_t>& gauges,
                             double elapsed_s, double interval_s);
    static uint64_t SteadyUs();
private:
    std::atomic<bool> enabled_{false};
    std::array<std::atomic<uint64_t>, static_cast<size_t>(Counter::Count)> counters_{};
    std::array<Histogram, static_cast<size_t>(Distribution::Count)> histograms_;
};

class ScopedMetric {
public:
    explicit ScopedMetric(Distribution metric)
        : metric_(metric), start_(metric != Distribution::Count && Metrics::Instance().Enabled() ? Metrics::SteadyUs() : 0) {}
    ~ScopedMetric() {
        if (start_) Metrics::Instance().Observe(metric_, Metrics::SteadyUs() - start_);
    }
private:
    Distribution metric_;
    uint64_t start_;
};

// One observer thread; JSONL output is independent of optional spdlog.
class MetricsReporter {
public:
    using Gauges = std::function<std::map<std::string, int64_t>()>;
    ~MetricsReporter();
    bool Start(const std::string& path, Gauges gauges, int interval_ms = 1000);
    void Stop();
private:
    std::ofstream output_;
    std::thread worker_;
    std::mutex mutex_;
    std::condition_variable wake_;
    bool stopped_ = true;
};
} // namespace game
