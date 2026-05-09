#pragma once

#include "storage/mysql_client.h"

#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_set>

namespace game {

struct BattleRecord {
    int64_t battle_id = 0;
    int64_t room_id = 0;
    int64_t winner_id = 0;
    int64_t loser_id = 0;
    std::string result_json;
};

struct SettlementLog {
    int64_t settlement_id = 0;
    int64_t battle_id = 0;
    int64_t player_id = 0;
    int score_delta = 0;
};

class BattleRepository {
public:
    explicit BattleRepository(MysqlClient* mysql = nullptr);

    bool InsertBattleRecord(const BattleRecord& record);
    bool InsertSettlementLog(const SettlementLog& log);
    bool HasSettlement(int64_t battle_id, int64_t player_id);

private:
    static std::string SettlementKey(int64_t battle_id, int64_t player_id);

    MysqlClient* mysql_;
    std::mutex mutex_;
    std::unordered_set<int64_t> battle_ids_;
    std::unordered_set<std::string> settlement_keys_;
};

} // namespace game

