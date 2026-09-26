#include "bot_manager.h"

#include <iostream>
#include <utility>

namespace game {

BotManager::BotManager(std::string host, uint16_t port, BotOptions options)
    : host_(std::move(host)),
      port_(port),
      options_(options),
      stats_(std::make_shared<BotStats>()) {}

void BotManager::Start(int bot_count) {
    bots_.reserve(bot_count);
    for (int i = 0; i < bot_count; ++i) {
        auto bot = std::make_unique<BotClient>(host_, port_, i, options_, stats_);
        bot->Start();
        bots_.push_back(std::move(bot));
    }
}

void BotManager::Stop() {
    for (auto& bot : bots_) {
        bot->Stop();
    }
    bots_.clear();
}

std::map<std::string, int64_t> BotManager::MeasurementGauges() const {
    return {
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
