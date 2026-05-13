#include "storage/battle_repository.h"

#include <sstream>

namespace game {

BattleRepository::BattleRepository(MysqlClient* mysql) : mysql_(mysql) {}

bool BattleRepository::InsertBattle(const Battle& battle) {
#if defined(GAME_USE_MYSQL) && GAME_USE_MYSQL
    if (mysql_ && mysql_->IsConnected()) {
        const auto result_json = mysql_->EscapeString(battle.result_json);
        std::ostringstream sql;
        sql << "INSERT IGNORE INTO battle("
            << "battle_id, room_id, winner_id, result_json) VALUES("
            << battle.battle_id << ", " << battle.room_id << ", "
            << battle.winner_id << ", '" << result_json << "')";
        return mysql_->ExecuteAffected(sql.str()) >= 0;
    }
#endif

    std::lock_guard<std::mutex> lock(mutex_);
    if (!battle_ids_.insert(battle.battle_id).second) {
        return false;
    }
    return mysql_ == nullptr || mysql_->Execute("insert battle");
}

bool BattleRepository::InsertBattlePlayerResult(const BattlePlayerResult& result) {
#if defined(GAME_USE_MYSQL) && GAME_USE_MYSQL
    if (mysql_ && mysql_->IsConnected()) {
        const auto escaped_result = mysql_->EscapeString(result.result);
        std::ostringstream sql;
        sql << "INSERT IGNORE INTO battle_player_result("
            << "battle_id, player_id, result, score_delta) VALUES("
            << result.battle_id << ", " << result.player_id << ", '"
            << escaped_result << "', " << result.score_delta << ")";
        return mysql_->ExecuteAffected(sql.str()) >= 0;
    }
#endif

    std::lock_guard<std::mutex> lock(mutex_);
    if (!battle_player_keys_.insert(BattlePlayerKey(result.battle_id, result.player_id)).second) {
        return false;
    }
    return mysql_ == nullptr || mysql_->Execute("insert battle player result");
}

bool BattleRepository::InsertSettlementLog(const SettlementLog& log) {
#if defined(GAME_USE_MYSQL) && GAME_USE_MYSQL
    if (mysql_ && mysql_->IsConnected()) {
        std::ostringstream sql;
        sql << "INSERT IGNORE INTO settlement_log("
            << "settlement_id, battle_id, player_id, score_delta) VALUES("
            << log.settlement_id << ", " << log.battle_id << ", "
            << log.player_id << ", " << log.score_delta << ")";
        return mysql_->ExecuteAffected(sql.str()) > 0;
    }
#endif

    std::lock_guard<std::mutex> lock(mutex_);
    if (!settlement_keys_.insert(SettlementKey(log.battle_id, log.player_id)).second) {
        return false;
    }
    return mysql_ == nullptr || mysql_->Execute("insert settlement log");
}

bool BattleRepository::HasSettlement(int64_t battle_id, int64_t player_id) {
#if defined(GAME_USE_MYSQL) && GAME_USE_MYSQL
    if (mysql_ && mysql_->IsConnected()) {
        std::ostringstream sql;
        sql << "SELECT settlement_id FROM settlement_log WHERE battle_id = "
            << battle_id << " AND player_id = " << player_id << " LIMIT 1";
        return !mysql_->Query(sql.str()).empty();
    }
#endif

    std::lock_guard<std::mutex> lock(mutex_);
    return settlement_keys_.find(SettlementKey(battle_id, player_id)) != settlement_keys_.end();
}

std::string BattleRepository::SettlementKey(int64_t battle_id, int64_t player_id) {
    return std::to_string(battle_id) + ":" + std::to_string(player_id);
}

std::string BattleRepository::BattlePlayerKey(int64_t battle_id, int64_t player_id) {
    return std::to_string(battle_id) + ":" + std::to_string(player_id);
}

} // namespace game
