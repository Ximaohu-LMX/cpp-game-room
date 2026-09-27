#include "bot_manager.h"

#include <iostream>
#include <utility>
#include <stdexcept>

namespace game {

BotManager::BotManager(std::string host, uint16_t port, BotOptions options)
    : host_(std::move(host)),
      port_(port),
      options_(options),
      stats_(std::make_shared<BotStats>()) {}

void BotManager::Start(int bot_count) {
    if (started_) throw std::logic_error("bot manager already started");
    if (bot_count <= 0 || options_.io_threads <= 0 || options_.input_interval_ms <= 0 ||
        options_.recovery_timeout_ms <= 0) throw std::invalid_argument("positive count, io threads and intervals required");
    // Resolve once before starting the fixed worker pool (no per-bot resolver threads).
    const auto endpoints = boost::asio::ip::tcp::resolver(io_context_).resolve(host_, std::to_string(port_));
    io_context_.restart();
    work_guard_ = std::make_unique<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>>(io_context_.get_executor());
    started_ = true;
    try {
        bots_.reserve(bot_count);
        for (int i = 0; i < bot_count; ++i) {
            auto bot = std::make_shared<BotClient>(io_context_, endpoints, i, options_, stats_);
            bots_.push_back(bot);
            bot->Start();
        }
        for (int i = 0; i < options_.io_threads; ++i) workers_.emplace_back([this] {
            ++stats_->workers_live;
            for (;;) {
                try { io_context_.run(); break; }
                catch (const std::exception& e) {
                    ++stats_->worker_exceptions;
                    std::cerr << "bot worker callback failed: " << e.what() << '\n';
                }
            }
            --stats_->workers_live;
        });
    } catch (...) { Stop(); throw; }
}

void BotManager::Stop() {
    if (!started_) return;
    for (const auto& bot : bots_) bot->Stop();
    work_guard_->reset();
    // Do not stop io_context: cancellation handlers must drain before buffers/bots die.
    if (workers_.empty()) io_context_.run();
    for (auto& worker : workers_) if (worker.joinable()) worker.join();
    workers_.clear();
    bots_.clear();
    work_guard_.reset();
    started_ = false;
}

std::map<std::string, int64_t> BotManager::MeasurementGauges() const {
    return {
        {"io_threads_configured", options_.io_threads},
        {"io_workers_live", stats_->workers_live.load()},
        {"worker_exceptions_total", stats_->worker_exceptions.load()},
        {"input_dropped_total", stats_->input_dropped.load()},
        {"input_cancelled_total", stats_->input_cancelled.load()},
        {"input_pending", stats_->input_pending.load()},
        {"reconnect_attempts_total", stats_->reconnect_attempts.load()},
        {"recovery_state_total", stats_->recovery_state.load()},
        {"recovery_no_room_total", stats_->recovery_no_room.load()},
        {"recovery_rejected_total", stats_->recovery_rejected.load()},
        {"recovery_timeout_total", stats_->recovery_timeout.load()},
        {"recovery_cancelled_total", stats_->recovery_cancelled.load()},
        {"recovery_terminal_total", stats_->recovery_terminal.load()},
        {"recovery_invalid_total", stats_->recovery_invalid.load()},
        {"recovery_pending", stats_->recovery_pending.load()},
        {"connections_live", stats_->connections_live.load()},
        {"connect_ok_total", stats_->connect_ok.load()},
        {"connect_failed_total", stats_->connect_failed.load()},
        {"login_attempts_total", stats_->login_attempts.load()},
        {"login_ok_total", stats_->login_ok.load()},
        {"login_failed_total", stats_->login_failed.load()},
        {"reconnect_ok_total", stats_->reconnect_ok.load()},
        {"reconnect_failed_total", stats_->reconnect_failed.load()},
        {"simulated_disconnects_total", stats_->disconnects.load()},
        {"unexpected_disconnects_total", stats_->unexpected_disconnects.load()},
        {"match_notifications_total", stats_->match_success.load()},
        {"playing_notifications_total", stats_->room_playing.load()},
        {"game_over_notifications_total", stats_->game_over.load()},
        {"input_enqueued_total", stats_->input_sent.load()},
        {"input_written_total", stats_->input_written.load()},
        {"packets_written_total", stats_->packets_written.load()},
        {"state_notifications_total", stats_->game_state.load()},
        {"heartbeat_sent_total", stats_->heartbeat_sent.load()},
        {"heartbeat_responses_total", stats_->heartbeat_responses.load()},
        {"heartbeat_timeouts_total", stats_->heartbeat_timeouts.load()},
        {"heartbeat_cancelled_total", stats_->heartbeat_cancelled.load()},
        {"heartbeat_pending", stats_->heartbeat_pending.load()}
    };
}

void BotManager::PrintStats(int64_t elapsed_seconds) const {
    if (!stats_) {
        return;
    }

    std::cout << "[t=" << elapsed_seconds << "s]"
              << " live=" << stats_->connections_live.load()
              << " connect_total=" << stats_->connect_ok.load()
              << " unexpected_disconnect=" << stats_->unexpected_disconnects.load()
              << " conn_fail=" << stats_->connect_failed.load()
              << " login=" << stats_->login_ok.load()
              << " login_fail=" << stats_->login_failed.load()
              << " reconnect=" << stats_->reconnect_ok.load()
              << " reconnect_fail=" << stats_->reconnect_failed.load()
              << " disconnect=" << stats_->disconnects.load()
              << " match=" << stats_->match_ok.load()
              << " match_fail=" << stats_->match_failed.load()
              << " cancel=" << stats_->match_cancel.load()
              << " match_notifications=" << stats_->match_success.load()
              << " ready=" << stats_->ready.load()
              << " unready=" << stats_->unready.load()
              << " playing_notifications=" << stats_->room_playing.load()
              << " input_enqueued=" << stats_->input_sent.load()
              << " state=" << stats_->game_state.load()
              << " over_notifications=" << stats_->game_over.load()
              << "\n";
}

} // namespace game
