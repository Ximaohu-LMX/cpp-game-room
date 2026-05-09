#pragma once

#include "net/session.h"
#include "storage/redis_client.h"

#include <cstdint>
#include <string>
#include <vector>

namespace game {

class MessageDispatcher;

struct RankEntry {
    int64_t player_id = 0;
    std::string name;
    int score = 0;
    int rank = 0;
};

class RankService {
public:
    explicit RankService(RedisClient* redis = nullptr);

    bool Init();
    void RegisterHandlers(MessageDispatcher& dispatcher);

    void UpdateScore(int64_t player_id, int score);
    std::vector<RankEntry> GetTopN(int offset, int limit);

    void HandleRankRequest(const SessionPtr& session, const Packet& packet);

private:
    RedisClient* redis_;
};

} // namespace game

