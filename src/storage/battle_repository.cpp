#include "storage/battle_repository.h"

#include <sstream>

namespace game {

BattleRepository::BattleRepository(MysqlClient* mysql) : mysql_(mysql) {}

bool BattleRepository::InsertBattleRecord(const BattleRecord& record) {
#if defined(GAME_USE_MYSQL) && GAME_USE_MYSQL
    if (mysql_ && mysql_->IsConnected()) {
        const auto result_json = mysql_->EscapeString(record.result_json);
        std::ostringstream sql;
        sql << "INSERT IGNORE INTO battle_record("
            << "battle_id, room_id, winner_id, loser_id, result_json) VALUES("
            << record.battle_id << ", " << record.room_id << ", "
            << record.winner_id << ", " << record.loser_id << ", '"
            << result_json << "')";
        return mysql_->ExecuteAffected(sql.str()) >= 0;
    }
#endif

    std::lock_guard<std::mutex> lock(mutex_);
    if (!battle_ids_.insert(record.battle_id).second) {
        return false;
    }
    return mysql_ == nullptr || mysql_->Execute("insert battle record");
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

} // namespace game
