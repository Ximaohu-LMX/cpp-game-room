#include "util/metrics.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace game {
namespace {
constexpr std::array<const char*, static_cast<size_t>(Counter::Count)> kCounters{{
    "connections_accepted", "connections_removed", "messages_received", "unknown_messages",
    "read_errors", "write_errors", "received_bytes", "sent_bytes", "sent_packets",
    "inputs_received", "inputs_consumed", "inputs_ignored", "rooms_created", "rooms_removed",
    "battles_started", "battles_finished", "settlement_attempts", "settlement_failures",
    "settlement_success", "loop_iterations", "tick_over_budget"
}};
constexpr std::array<const char*, static_cast<size_t>(Distribution::Count)> kDistributions{{
    "loop_work_us", "loop_interval_us", "room_tick_us", "tick_callback_us",
    "message_handler_us", "login_handler_us", "input_queue_wait_us", "battle_duration_us",
    "settlement_us", "bot_login_rtt_us", "bot_heartbeat_rtt_us"
}};
}

double HistogramSnapshot::UpperBound(size_t index) {
    return std::pow(1.1, static_cast<double>(index));
}
double HistogramSnapshot::Percentile(double quantile) const {
    if (!count) return 0;
    const auto rank = static_cast<uint64_t>(std::ceil(std::clamp(quantile, 0.0, 1.0) * count));
    uint64_t seen = 0;
    for (size_t i = 0; i < buckets.size(); ++i) {
        seen += buckets[i];
        if (seen >= std::max<uint64_t>(1, rank))
            return i + 1 == buckets.size() ? max_us : std::min<double>(max_us, UpperBound(i));
    }
    return max_us;
}
void Histogram::Observe(uint64_t microseconds) {
    size_t bucket = 0;
    if (microseconds > 1)
        bucket = std::min<size_t>(HistogramSnapshot::kBuckets - 1,
            static_cast<size_t>(std::ceil(std::log(static_cast<double>(microseconds)) / std::log(1.1))));
    std::lock_guard<std::mutex> lock(mutex_);
    ++data_.buckets[bucket];
    ++data_.count;
    data_.sum_us += microseconds;
    data_.max_us = std::max(data_.max_us, microseconds);
}
HistogramSnapshot Histogram::Drain() {
    std::lock_guard<std::mutex> lock(mutex_);
    auto result = data_;
    data_ = {};
    return result;
}
Metrics& Metrics::Instance() {
    static Metrics metrics;
    return metrics;
}
uint64_t Metrics::SteadyUs() {
    return std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}
void Metrics::Increment(Counter metric, uint64_t amount) {
    if (Enabled()) counters_[static_cast<size_t>(metric)].fetch_add(amount, std::memory_order_relaxed);
}
void Metrics::Observe(Distribution metric, uint64_t microseconds) {
    if (Enabled() && metric != Distribution::Count) histograms_[static_cast<size_t>(metric)].Observe(microseconds);
}
std::string Metrics::SnapshotJson(const std::map<std::string, int64_t>& gauges,
                                  double elapsed_s, double interval_s) {
    std::ostringstream out;
    out << std::setprecision(12);
    const auto unix_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    out << "{\"schema\":1,\"unix_ms\":" << unix_ms << ",\"elapsed_s\":" << elapsed_s
        << ",\"interval_s\":" << interval_s << ",\"gauges\":{";
    bool first = true;
    for (const auto& [name, value] : gauges) {
        if (!first) out << ',';
        first = false;
        out << '"' << name << "\":" << value;
    }
    out << "},\"counters\":{";
    for (size_t i = 0; i < kCounters.size(); ++i) {
        if (i) out << ',';
        out << '"' << kCounters[i] << "\":" << counters_[i].load(std::memory_order_relaxed);
    }
    out << "},\"histograms\":{";
    for (size_t i = 0; i < kDistributions.size(); ++i) {
        if (i) out << ',';
        const auto h = histograms_[i].Drain();
        out << '"' << kDistributions[i] << "\":{\"count\":" << h.count
            << ",\"sum_us\":" << h.sum_us << ",\"max_us\":" << h.max_us
            << ",\"p50_us\":" << h.Percentile(0.50) << ",\"p95_us\":" << h.Percentile(0.95)
            << ",\"p99_us\":" << h.Percentile(0.99) << ",\"buckets\":[";
        bool first_bucket = true;
        for (size_t b = 0; b < h.buckets.size(); ++b) {
            if (!h.buckets[b]) continue;
            if (!first_bucket) out << ',';
            first_bucket = false;
            out << '[' << b << ',' << h.buckets[b] << ']';
        }
        out << "]}";
    }
    out << "}}";
    return out.str();
}
MetricsReporter::~MetricsReporter() { Stop(); }
bool MetricsReporter::Start(const std::string& path, Gauges gauges, int interval_ms) {
    if (worker_.joinable() || interval_ms <= 0) return false;
    output_.open(path, std::ios::out | std::ios::trunc);
    if (!output_) return false;
    stopped_ = false;
    Metrics::Instance().Enable(true);
    worker_ = std::thread([this, gauges = std::move(gauges), interval_ms]() {
        const auto start = Metrics::SteadyUs();
        auto previous = start;
        std::unique_lock<std::mutex> lock(mutex_);
        while (!stopped_) {
            wake_.wait_for(lock, std::chrono::milliseconds(interval_ms), [this]() { return stopped_; });
            const auto now = Metrics::SteadyUs();
            output_ << Metrics::Instance().SnapshotJson(gauges(), (now - start) / 1e6,
                                                       (now - previous) / 1e6) << '\n';
            output_.flush();
            previous = now;
        }
    });
    return true;
}
void MetricsReporter::Stop() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stopped_ = true;
    }
    wake_.notify_all();
    if (worker_.joinable()) worker_.join();
    output_.close();
    Metrics::Instance().Enable(false);
}
} // namespace game
