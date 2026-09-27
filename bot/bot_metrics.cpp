#include "bot_metrics.h"
#include <iomanip>
#include <sstream>

namespace game {
bool BotStats::OpenRecoveryLog(const std::string& path) {
    if (path.empty()) return true;
    recovery_log_.open(path, std::ios::out | std::ios::trunc);
    return static_cast<bool>(recovery_log_);
}
void BotStats::RecoveryEvent(const std::string& json) {
    std::lock_guard<std::mutex> lock(event_mutex_);
    if (recovery_log_.is_open()) { recovery_log_ << json << '\n'; recovery_log_.flush(); }
}
namespace {
void WriteHistogram(std::ostream& out, Histogram& histogram) {
    const auto h = histogram.Drain();
    out << "{\"count\":" << h.count << ",\"sum_us\":" << h.sum_us
        << ",\"max_us\":" << h.max_us << ",\"p50_us\":" << h.Percentile(.5)
        << ",\"p95_us\":" << h.Percentile(.95) << ",\"p99_us\":" << h.Percentile(.99)
        << ",\"buckets\":[";
    bool first = true;
    for (size_t i = 0; i < h.buckets.size(); ++i) if (h.buckets[i]) {
        if (!first) out << ',';
        first = false;
        out << '[' << i << ',' << h.buckets[i] << ']';
    }
    out << "]}";
}
}
bool BotMetricsReporter::Start(const std::string& path, std::shared_ptr<BotStats> stats,
                              std::function<std::map<std::string, int64_t>()> gauges) {
    if (worker_.joinable()) return false;
    output_.open(path, std::ios::out | std::ios::trunc);
    if (!output_) return false;
    stats_ = std::move(stats);
    stats_->measurements = true;
    stopped_ = false;
    worker_ = std::thread([this, gauges = std::move(gauges)] {
        const auto start = Metrics::SteadyUs();
        auto previous = start;
        std::unique_lock<std::mutex> lock(mutex_);
        while (!stopped_) {
            wake_.wait_for(lock, std::chrono::seconds(1), [this] { return stopped_; });
            const auto now = Metrics::SteadyUs();
            const auto unix_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            output_ << std::setprecision(12) << "{\"schema\":1,\"unix_ms\":" << unix_ms
                << ",\"elapsed_s\":" << (now - start) / 1e6
                << ",\"interval_s\":" << (now - previous) / 1e6 << ",\"gauges\":{";
            bool first = true;
            for (const auto& [name, value] : gauges()) {
                if (!first) output_ << ',';
                first = false;
                output_ << '"' << name << "\":" << value;
            }
            output_ << "},\"counters\":{},\"histograms\":{";
            const std::pair<const char*, Histogram*> histograms[] = {
                {"bot_login_rtt_us", &stats_->login_rtt},
                {"bot_heartbeat_rtt_us", &stats_->heartbeat_rtt},
                {"bot_input_timer_late_us", &stats_->input_timer_late},
                {"bot_input_post_late_us", &stats_->input_post_late},
                {"bot_input_write_wait_us", &stats_->input_write_wait},
                {"bot_input_interval_us", &stats_->input_interval},
                {"bot_reconnect_rtt_us", &stats_->reconnect_rtt},
                {"bot_recovery_state_us", &stats_->recovery_latency}};
            first = true;
            for (const auto& [name, histogram] : histograms) {
                if (!first) output_ << ',';
                first = false;
                output_ << '"' << name << "\":";
                WriteHistogram(output_, *histogram);
            }
            output_ << "}}\n";
            output_.flush();
            previous = now;
        }
    });
    return true;
}
void BotMetricsReporter::Stop() {
    { std::lock_guard<std::mutex> lock(mutex_); stopped_ = true; }
    wake_.notify_all();
    if (worker_.joinable()) worker_.join();
    if (stats_) stats_->measurements = false;
    output_.close();
}
} // namespace game
