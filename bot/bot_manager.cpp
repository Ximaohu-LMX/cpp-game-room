#include "bot_manager.h"

#include <utility>

namespace game {

BotManager::BotManager(std::string host, uint16_t port) : host_(std::move(host)), port_(port) {}

void BotManager::Start(int bot_count) {
    bots_.reserve(bot_count);
    for (int i = 0; i < bot_count; ++i) {
        auto bot = std::make_unique<BotClient>(host_, port_, i);
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

} // namespace game
