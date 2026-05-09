#include "storage/battle_repository.h"

namespace game {

BattleRepository::BattleRepository(MysqlClient* mysql) : mysql_(mysql) {}

bool BattleRepository::InsertBattleRecord(const BattleRecord& record) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!battle_ids_.insert(record.battle_id).second) {
        return false;
    }
    return mysql_ == nullptr || mysql_->Execute("insert battle record");
}

bool BattleRepository::InsertSettlementLog(const SettlementLog& log) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!settlement_keys_.insert(SettlementKey(log.battle_id, log.player_id)).second) {
        return false;
    }
    return mysql_ == nullptr || mysql_->Execute("insert settlement log");
}

bool BattleRepository::HasSettlement(int64_t battle_id, int64_t player_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    return settlement_keys_.find(SettlementKey(battle_id, player_id)) != settlement_keys_.end();
}

std::string BattleRepository::SettlementKey(int64_t battle_id, int64_t player_id) {
    return std::to_string(battle_id) + ":" + std::to_string(player_id);
}

} // namespace game

