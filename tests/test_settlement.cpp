#include "game/settlement_service.h"
#include "rank/rank_service.h"
#include "storage/battle_repository.h"
#include "storage/player_repository.h"
#include "storage/redis_client.h"

#include <gtest/gtest.h>

TEST(SettlementTest, SettlementLogIsIdempotentByBattlePlayer) {
    game::BattleRepository battle_repo;
    game::SettlementLog log;
    log.battle_id = 1;
    log.player_id = 100;
    log.score_delta = 20;
    EXPECT_TRUE(battle_repo.InsertSettlementLog(log));
    EXPECT_FALSE(battle_repo.InsertSettlementLog(log));
}

TEST(SettlementTest, UpdateRankAfterSettle) {
    game::RedisClient redis;
    game::PlayerRepository players;
    game::BattleRepository battles;
    game::RankService rank(&redis);
    game::SettlementService service(&battles, &players, &rank);
    service.SettleBattle(1, 10, {20});

    auto items = rank.GetTopN(0, 10);
    ASSERT_FALSE(items.empty());
    EXPECT_EQ(items[0].player_id, 10);
}

