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

void BotManager::PrintStats(int64_t elapsed_seconds) const {
    if (!stats_) {
        return;
    }

    std::cout << "[t=" << elapsed_seconds << "s]"
              << " conn=" << stats_->connect_ok.load()
              << " conn_fail=" << stats_->connect_failed.load()
              << " login=" << stats_->login_ok.load()
              << " login_fail=" << stats_->login_failed.load()
              << " reconnect=" << stats_->reconnect_ok.load()
              << " reconnect_fail=" << stats_->reconnect_failed.load()
              << " disconnect=" << stats_->disconnects.load()
              << " match=" << stats_->match_ok.load()
              << " match_fail=" << stats_->match_failed.load()
              << " cancel=" << stats_->match_cancel.load()
              << " rooms=" << stats_->match_success.load()
              << " ready=" << stats_->ready.load()
              << " unready=" << stats_->unready.load()
              << " playing=" << stats_->room_playing.load()
              << " input=" << stats_->input_sent.load()
              << " state=" << stats_->game_state.load()
              << " over=" << stats_->game_over.load()
              << "\n";
}

} // namespace game
