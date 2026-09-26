#include "game/settlement_service.h"

#include "rank/rank_service.h"
#include "util/logger.h"

#include <set>

namespace game {

SettlementService::SettlementService(BattleRepository* battle_repository, PlayerRepository* player_repository, RankService* rank_service)
    : battle_repository_(battle_repository), player_repository_(player_repository), rank_service_(rank_service) {}

bool SettlementService::SettleBattle(int64_t battle_id, int64_t room_id, int64_t winner_id,
                                    const std::vector<int64_t>& losers) {
    if (!battle_repository_ || (rank_service_ && !player_repository_) ||
        battle_id <= 0 || room_id <= 0 || winner_id <= 0) {
        return false;
    }

    std::set<int64_t> participants{winner_id};
    for (auto player_id : losers) {
        if (player_id <= 0 || player_id == winner_id) {
            return false;
        }
        participants.insert(player_id);
    }

    Battle battle;
    battle.battle_id = battle_id;
    battle.room_id = room_id;
    battle.winner_id = winner_id;
    std::vector<BattlePlayerResult> results;
    for (auto player_id : participants) {
        BattlePlayerResult result;
        result.battle_id = battle_id;
        result.player_id = player_id;
        result.result = player_id == winner_id ? "WIN" : "LOSE";
        result.score_delta = player_id == winner_id ? 20 : -10;
        results.push_back(result);
    }

    std::lock_guard<std::mutex> lock(mutex_);
    if (!battle_repository_->ApplySettlement(battle, results)) {
        LOG_ERROR("settlement failed or conflicted, battle {}, room {}", battle_id, room_id);
        return false;
    }

    // 重试命中已结算记录时也刷新榜单；读取 MySQL 当前总分，用 ZADD 覆盖而非重复增量加分。
    if (rank_service_) {
        for (auto player_id : participants) {
            PlayerData data;
            if (!player_repository_->FindPlayer(player_id, data) ||
                !rank_service_->UpdateScore(player_id, data.score)) {
                LOG_WARN("rank sync pending, battle {}, player {}", battle_id, player_id);
                return false;
            }
        }
    }
    LOG_INFO("settlement complete, battle {}, room {}, winner {}", battle_id, room_id, winner_id);
    return true;
}

} // namespace game
