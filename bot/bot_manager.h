#pragma once

#include "bot_client.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace game {

class BotManager {
public:
    BotManager(std::string host, uint16_t port);

    void Start(int bot_count);
    void Stop();

private:
    std::string host_;
    uint16_t port_;
    std::vector<std::unique_ptr<BotClient>> bots_;
};

} // namespace game

