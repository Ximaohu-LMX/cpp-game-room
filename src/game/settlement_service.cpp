#include "game/settlement_service.h"

#include "rank/rank_service.h"
#include "util/id_generator.h"
#include "util/logger.h"

namespace game {

SettlementService::SettlementService(BattleRepository* battle_repository, PlayerRepository* player_repository, RankService* rank_service)
    : battle_repository_(battle_repository), player_repository_(player_repository), rank_service_(rank_service) {}

void SettlementService::SettleBattle(int64_t room_id, int64_t winner_id, const std::vector<int64_t>& losers) {
    const int64_t battle_id = IdGenerator::NextBattleId();

    if (battle_repository_) {
        Battle battle;
        battle.battle_id = battle_id;
        battle.room_id = room_id;
        battle.winner_id = winner_id;
        battle.result_json = "{}";
        battle_repository_->InsertBattle(battle);
    }

    if (battle_repository_) {
        BattlePlayerResult winner_result;
        winner_result.battle_id = battle_id;
        winner_result.player_id = winner_id;
        winner_result.result = "WIN";
        winner_result.score_delta = 20;
        battle_repository_->InsertBattlePlayerResult(winner_result);
    }
    if (TryInsertSettlementLog(battle_id, winner_id, 20)) {
        UpdatePlayerScore(winner_id, 20);
    }
    for (auto loser_id : losers) {
        if (battle_repository_) {
            BattlePlayerResult loser_result;
            loser_result.battle_id = battle_id;
            loser_result.player_id = loser_id;
            loser_result.result = "LOSE";
            loser_result.score_delta = -10;
            battle_repository_->InsertBattlePlayerResult(loser_result);
        }
        if (TryInsertSettlementLog(battle_id, loser_id, -10)) {
            UpdatePlayerScore(loser_id, -10);
        }
    }
    LOG_INFO("settled battle {}, room {}, winner {}", battle_id, room_id, winner_id);
}

bool SettlementService::TryInsertSettlementLog(int64_t battle_id, int64_t player_id, int delta) {
    if (!battle_repository_) {
        return true;
    }
    SettlementLog log;
    log.settlement_id = IdGenerator::NextBattleId();
    log.battle_id = battle_id;
    log.player_id = player_id;
    log.score_delta = delta;
    return battle_repository_->InsertSettlementLog(log);
}

void SettlementService::UpdatePlayerScore(int64_t player_id, int delta) {
    if (player_repository_) {
        player_repository_->UpdatePlayerScore(player_id, delta);
        auto data = player_repository_->LoadPlayer(player_id);
        if (rank_service_) {
            rank_service_->UpdateScore(player_id, data.score);
        }
    }
}

} // namespace game
