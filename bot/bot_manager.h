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

    ~BotManager() { Stop(); }
    void Start(int bot_count);
    std::shared_ptr<BotStats> Stats() const { return stats_; }
    void Stop();
    void PrintStats(int64_t elapsed_seconds) const;
    std::map<std::string, int64_t> MeasurementGauges() const;

private:
    std::string host_;
    uint16_t port_;
    BotOptions options_;
    std::shared_ptr<BotStats> stats_;
    boost::asio::io_context io_context_;
    std::unique_ptr<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>> work_guard_;
    std::vector<std::shared_ptr<BotClient>> bots_;
    std::vector<std::thread> workers_;
    bool started_ = false;
};

} // namespace game
