#pragma once

#include "bot_client.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace game {

class BotManager {
public:
    BotManager(std::string host, uint16_t port, BotOptions options);

    void Start(int bot_count);
    void Stop();
    void PrintStats(int64_t elapsed_seconds) const;

private:
    std::string host_;
    uint16_t port_;
    BotOptions options_;
    std::shared_ptr<BotStats> stats_;
    std::vector<std::unique_ptr<BotClient>> bots_;
};

} // namespace game
