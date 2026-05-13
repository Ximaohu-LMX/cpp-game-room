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

TEST(SettlementTest, BattleHasOneRowAndPlayerResultsArePerPlayer) {
    game::BattleRepository battle_repo;
    game::Battle battle;
    battle.battle_id = 1;
    battle.room_id = 10;
    battle.winner_id = 100;
    battle.result_json = "{}";

    EXPECT_TRUE(battle_repo.InsertBattle(battle));
    EXPECT_FALSE(battle_repo.InsertBattle(battle));

    game::BattlePlayerResult winner;
    winner.battle_id = 1;
    winner.player_id = 100;
    winner.result = "WIN";
    winner.score_delta = 20;

    game::BattlePlayerResult loser;
    loser.battle_id = 1;
    loser.player_id = 101;
    loser.result = "LOSE";
    loser.score_delta = -10;

    EXPECT_TRUE(battle_repo.InsertBattlePlayerResult(winner));
    EXPECT_TRUE(battle_repo.InsertBattlePlayerResult(loser));
    EXPECT_FALSE(battle_repo.InsertBattlePlayerResult(loser));
}

TEST(SettlementTest, UpdateRankAfterSettle) {
    game::RedisClient redis(true);
    game::PlayerRepository players;
    game::BattleRepository battles;
    game::RankService rank(&redis);
    game::SettlementService service(&battles, &players, &rank);
    service.SettleBattle(1, 10, {20});

    auto items = rank.GetTopN(0, 10);
    ASSERT_FALSE(items.empty());
    EXPECT_EQ(items[0].player_id, 10);
}
