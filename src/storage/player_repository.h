#pragma once

#include "storage/mysql_client.h"

#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>

namespace game {

struct PlayerData {
    int64_t player_id = 0;
    std::string name;
    int score = 1000;
    int win_count = 0;
    int lose_count = 0;
};

class PlayerRepository {
public:
    explicit PlayerRepository(MysqlClient* mysql = nullptr);

    PlayerData LoadPlayer(int64_t player_id);
    bool CreatePlayer(const PlayerData& data);
    bool UpdatePlayerScore(int64_t player_id, int score_delta);

private:
    MysqlClient* mysql_;
    std::mutex mutex_;
    std::unordered_map<int64_t, PlayerData> players_;
};

} // namespace game

