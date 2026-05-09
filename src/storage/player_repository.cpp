#include "storage/player_repository.h"

namespace game {

PlayerRepository::PlayerRepository(MysqlClient* mysql) : mysql_(mysql) {}

PlayerData PlayerRepository::LoadPlayer(int64_t player_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = players_.find(player_id);
    if (it != players_.end()) {
        return it->second;
    }

    PlayerData data;
    data.player_id = player_id;
    data.name = "player_" + std::to_string(player_id);
    players_[player_id] = data;
    return data;
}

bool PlayerRepository::CreatePlayer(const PlayerData& data) {
    std::lock_guard<std::mutex> lock(mutex_);
    players_[data.player_id] = data;
    return mysql_ == nullptr || mysql_->Execute("insert player");
}

bool PlayerRepository::UpdatePlayerScore(int64_t player_id, int score_delta) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto& data = players_[player_id];
    data.player_id = player_id;
    if (data.name.empty()) {
        data.name = "player_" + std::to_string(player_id);
    }
    data.score += score_delta;
    if (score_delta > 0) {
        ++data.win_count;
    } else if (score_delta < 0) {
        ++data.lose_count;
    }
    return mysql_ == nullptr || mysql_->Execute("update player score");
}

} // namespace game

