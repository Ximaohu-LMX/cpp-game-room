#pragma once

#include "storage/battle_repository.h"
#include "storage/player_repository.h"

#include <cstdint>
#include <vector>

namespace game {

class RankService;

class SettlementService {
public:
    SettlementService(BattleRepository* battle_repository, PlayerRepository* player_repository, RankService* rank_service);

    void SettleBattle(int64_t room_id, int64_t winner_id, const std::vector<int64_t>& losers);

private:
    bool TryInsertSettlementLog(int64_t battle_id, int64_t player_id, int delta);
    void UpdatePlayerScore(int64_t player_id, int delta);

    BattleRepository* battle_repository_;
    PlayerRepository* player_repository_;
    RankService* rank_service_;
};

} // namespace game

