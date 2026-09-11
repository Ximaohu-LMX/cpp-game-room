#include "storage/battle_repository.h"

#include "storage/player_repository.h"
#include "util/logger.h"

#include <algorithm>
#include <sstream>

namespace game {

BattleRepository::BattleRepository(MysqlClient* mysql) : mysql_(mysql) {}

int64_t BattleRepository::CreateBattle(int64_t room_id) {
    if (!mysql_ || room_id <= 0) {
        return 0;
    }
    return mysql_->ExecuteInsert(
        "INSERT INTO battle(room_id, winner_id, result_json) VALUES(" +
        std::to_string(room_id) + ", 0, '{}')");
}

bool BattleRepository::ApplySettlement(const Battle& battle, const std::vector<BattlePlayerResult>& results) {
    if (!mysql_ || battle.battle_id <= 0 || battle.room_id <= 0 || battle.winner_id <= 0 || results.empty()) {
        return false;
    }
    auto ordered = results;
    std::sort(ordered.begin(), ordered.end(), [](const auto& a, const auto& b) {
        return a.player_id < b.player_id;
    });
    int winner_count = 0;
    int64_t previous_player = 0;
    for (const auto& result : ordered) {
        const bool winner = result.player_id == battle.winner_id;
        if (result.battle_id != battle.battle_id || result.player_id <= previous_player ||
            result.result != (winner ? "WIN" : "LOSE") || result.score_delta != (winner ? 20 : -10)) {
            return false;
        }
        winner_count += winner ? 1 : 0;
        previous_player = result.player_id;
    }
    if (winner_count != 1) {
        return false;
    }

    return mysql_->RunTransaction([&]() {
        // 数据库行锁让不同连接对同一 battle_id 的并发重试串行执行。
        const auto rows = mysql_->Query(
            "SELECT room_id, winner_id, settled FROM battle WHERE battle_id = " +
            std::to_string(battle.battle_id) + " FOR UPDATE");
        if (rows.size() != 1 || std::stoll(rows[0].at("room_id")) != battle.room_id) {
            return false;
        }
        if (rows[0].at("settled") == "1") {
            // 只有同一份结果才算成功重试，不能把不同胜者/参赛玩家的请求静默吞掉。
            if (std::stoll(rows[0].at("winner_id")) != battle.winner_id) {
                LOG_ERROR("conflicting settlement for battle {}", battle.battle_id);
                return false;
            }
            const auto saved = mysql_->Query(
                "SELECT player_id, result, score_delta FROM battle_player_result WHERE battle_id = " +
                std::to_string(battle.battle_id) + " ORDER BY player_id FOR UPDATE");
            if (saved.size() != ordered.size()) {
                return false;
            }
            for (size_t i = 0; i < ordered.size(); ++i) {
                if (std::stoll(saved[i].at("player_id")) != ordered[i].player_id ||
                    saved[i].at("result") != ordered[i].result ||
                    std::stoi(saved[i].at("score_delta")) != ordered[i].score_delta) {
                    return false;
                }
            }
            return true;
        }

        PlayerRepository players(mysql_);
        for (const auto& result : ordered) {
            SettlementLog log;
            log.battle_id = battle.battle_id;
            log.player_id = result.player_id;
            log.score_delta = result.score_delta;
            if (!InsertBattlePlayerResult(result) || !InsertSettlementLog(log) ||
                !players.UpdatePlayerScore(result.player_id, result.score_delta)) {
                return false;
            }
        }
        return mysql_->ExecuteAffected(
            "UPDATE battle SET winner_id = " + std::to_string(battle.winner_id) +
            ", settled = 1, end_time = CURRENT_TIMESTAMP WHERE battle_id = " +
            std::to_string(battle.battle_id)) == 1;
    });
}

bool BattleRepository::InsertBattle(const Battle& battle) {
    if (!mysql_) {
        return false;
    }
    const auto result_json = mysql_->EscapeString(battle.result_json);
    std::ostringstream sql;
    sql << "INSERT INTO battle("
        << "battle_id, room_id, winner_id, result_json) VALUES("
        << battle.battle_id << ", " << battle.room_id << ", "
        << battle.winner_id << ", '" << result_json << "')";
    return mysql_->ExecuteAffected(sql.str()) > 0;
}

bool BattleRepository::InsertBattlePlayerResult(const BattlePlayerResult& result) {
    if (!mysql_) {
        return false;
    }
    const auto escaped_result = mysql_->EscapeString(result.result);
    std::ostringstream sql;
    sql << "INSERT INTO battle_player_result("
        << "battle_id, player_id, result, score_delta) VALUES("
        << result.battle_id << ", " << result.player_id << ", '"
        << escaped_result << "', " << result.score_delta << ")";
    return mysql_->ExecuteAffected(sql.str()) > 0;
}

bool BattleRepository::InsertSettlementLog(const SettlementLog& log) {
    if (!mysql_) {
        return false;
    }
    std::ostringstream sql;
    sql << "INSERT INTO settlement_log(";
    if (log.settlement_id != 0) {
        sql << "settlement_id, ";
    }
    sql << "battle_id, player_id, score_delta) VALUES(";
    if (log.settlement_id != 0) {
        sql << log.settlement_id << ", ";
    }
    sql << log.battle_id << ", "
        << log.player_id << ", " << log.score_delta << ")";
    return mysql_->ExecuteAffected(sql.str()) > 0;
}

bool BattleRepository::HasSettlement(int64_t battle_id, int64_t player_id) {
    if (!mysql_) {
        return false;
    }
    std::ostringstream sql;
    sql << "SELECT settlement_id FROM settlement_log WHERE battle_id = "
        << battle_id << " AND player_id = " << player_id << " LIMIT 1";
    return !mysql_->Query(sql.str()).empty();
}

} // namespace game
