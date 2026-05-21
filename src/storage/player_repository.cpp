#include "storage/player_repository.h"

#include <cstdlib>
#include <sstream>

namespace game {

PlayerRepository::PlayerRepository(MysqlClient* mysql) : mysql_(mysql) {}

PlayerData PlayerRepository::LoadPlayer(int64_t player_id) {
    if (mysql_) {
        const auto rows = mysql_->Query(
            "SELECT player_id, name, score, win_count, lose_count FROM player WHERE player_id = " +
            std::to_string(player_id) + " LIMIT 1");
        if (!rows.empty()) {
            const auto& row = rows.front();
            PlayerData data;
            data.player_id = std::stoll(row.at("player_id"));
            data.name = row.at("name");
            data.score = std::stoi(row.at("score"));
            data.win_count = std::stoi(row.at("win_count"));
            data.lose_count = std::stoi(row.at("lose_count"));
            return data;
        }
    }

    PlayerData data;
    data.player_id = player_id;
    data.name = "player_" + std::to_string(player_id);
    CreatePlayer(data);
    return data;
}

bool PlayerRepository::CreatePlayer(const PlayerData& data) {
    if (!mysql_) {
        return false;
    }
    const auto name = mysql_->EscapeString(data.name);
    std::ostringstream sql;
    sql << "INSERT INTO player(player_id, name, score, win_count, lose_count) VALUES("
        << data.player_id << ", '" << name << "', " << data.score << ", "
        << data.win_count << ", " << data.lose_count << ") "
        << "ON DUPLICATE KEY UPDATE name = VALUES(name)";
    return mysql_->Execute(sql.str());
}

bool PlayerRepository::UpdatePlayerScore(int64_t player_id, int score_delta) {
    if (!mysql_) {
        return false;
    }
    const int win_delta = score_delta > 0 ? 1 : 0;
    const int lose_delta = score_delta < 0 ? 1 : 0;
    const int initial_score = 1000 + score_delta;
    std::ostringstream sql;
    sql << "INSERT INTO player(player_id, name, score, win_count, lose_count) VALUES("
        << player_id << ", 'player_" << player_id << "', " << initial_score << ", "
        << win_delta << ", " << lose_delta << ") "
        << "ON DUPLICATE KEY UPDATE "
        << "score = score + (" << score_delta << "), "
        << "win_count = win_count + " << win_delta << ", "
        << "lose_count = lose_count + " << lose_delta;
    return mysql_->Execute(sql.str());
}

} // namespace game
