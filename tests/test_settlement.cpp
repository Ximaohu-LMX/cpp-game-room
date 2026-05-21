#include "config/config_manager.h"
#include "game/settlement_service.h"
#include "rank/rank_service.h"
#include "storage/battle_repository.h"
#include "storage/player_repository.h"
#include "storage/redis_client.h"

#include <chrono>
#include <string>

#include <gtest/gtest.h>

namespace {

int64_t UniqueId(int suffix) {
    const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    return 800000000000LL + (now % 1000000000LL) * 10 + suffix;
}

void LoadTestConfig() {
    game::ConfigManager::Instance().Load(std::string(GAME_TEST_SOURCE_DIR) + "/config/server.yaml");
}

} // namespace

TEST(SettlementTest, SettlementLogIsIdempotentByBattlePlayer) {
    LoadTestConfig();
    game::MysqlClient mysql;
    if (!mysql.Connect()) {
        GTEST_SKIP() << "MySQL is not available";
    }
    game::BattleRepository battle_repo(&mysql);
    game::SettlementLog log;
    log.settlement_id = UniqueId(1);
    log.battle_id = UniqueId(2);
    log.player_id = UniqueId(3);
    log.score_delta = 20;
    EXPECT_TRUE(battle_repo.InsertSettlementLog(log));
    EXPECT_FALSE(battle_repo.InsertSettlementLog(log));
}

TEST(SettlementTest, BattleHasOneRowAndPlayerResultsArePerPlayer) {
    LoadTestConfig();
    game::MysqlClient mysql;
    if (!mysql.Connect()) {
        GTEST_SKIP() << "MySQL is not available";
    }
    game::BattleRepository battle_repo(&mysql);
    const auto battle_id = UniqueId(10);
    const auto winner_id = UniqueId(11);
    const auto loser_id = UniqueId(12);

    game::Battle battle;
    battle.battle_id = battle_id;
    battle.room_id = UniqueId(13);
    battle.winner_id = winner_id;
    battle.result_json = "{}";

    EXPECT_TRUE(battle_repo.InsertBattle(battle));
    EXPECT_FALSE(battle_repo.InsertBattle(battle));

    game::BattlePlayerResult winner;
    winner.battle_id = battle_id;
    winner.player_id = winner_id;
    winner.result = "WIN";
    winner.score_delta = 20;

    game::BattlePlayerResult loser;
    loser.battle_id = battle_id;
    loser.player_id = loser_id;
    loser.result = "LOSE";
    loser.score_delta = -10;

    EXPECT_TRUE(battle_repo.InsertBattlePlayerResult(winner));
    EXPECT_TRUE(battle_repo.InsertBattlePlayerResult(loser));
    EXPECT_FALSE(battle_repo.InsertBattlePlayerResult(loser));
}

TEST(SettlementTest, UpdateRankAfterSettle) {
    LoadTestConfig();
    game::MysqlClient mysql;
    game::RedisClient redis;
    if (!mysql.Connect()) {
        GTEST_SKIP() << "MySQL is not available";
    }
    if (!redis.Connect()) {
        GTEST_SKIP() << "Redis is not available";
    }
    game::PlayerRepository players(&mysql);
    game::BattleRepository battles(&mysql);
    game::RankService rank(&redis);
    game::SettlementService service(&battles, &players, &rank);
    const auto winner_id = UniqueId(20);
    const auto loser_id = UniqueId(21);
    game::PlayerData winner;
    winner.player_id = winner_id;
    winner.name = "player_" + std::to_string(winner_id);
    winner.score = 2147480000;
    ASSERT_TRUE(players.CreatePlayer(winner));

    service.SettleBattle(UniqueId(22), winner_id, {loser_id});

    auto items = rank.GetTopN(0, 10);
    ASSERT_FALSE(items.empty());
    EXPECT_EQ(items[0].player_id, winner_id);
}
