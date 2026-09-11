#include "config/config_manager.h"
#include "game/settlement_service.h"
#include "rank/rank_service.h"
#include "storage/battle_repository.h"
#include "storage/player_repository.h"
#include "storage/redis_client.h"
#include "test_config.h"

#include <atomic>
#include <chrono>
#include <future>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

#include <gtest/gtest.h>

namespace {

int64_t UniqueId() {
    static std::atomic<int64_t> next{
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count()};
    return next.fetch_add(1);
}

class SettlementTest : public ::testing::Test {
protected:
    void SetUp() override {
        ASSERT_TRUE(LoadIntegrationTestConfig());
        if (!mysql.Connect()) {
            GTEST_SKIP() << "MySQL is not available";
        }
        ASSERT_TRUE(mysql.Execute("SET SESSION sql_mode = 'STRICT_ALL_TABLES'"));
        room_id = UniqueId();
        winner_id = AddPlayer();
        loser_id = AddPlayer();
        battle_id = AddBattle();
        ASSERT_GT(battle_id, 0);
    }

    void TearDown() override {
        if (!mysql.IsConnected()) {
            return;
        }
        for (auto id : battle_ids) {
            mysql.Execute("DELETE FROM settlement_log WHERE battle_id = " + std::to_string(id));
            mysql.Execute("DELETE FROM battle_player_result WHERE battle_id = " + std::to_string(id));
            mysql.Execute("DELETE FROM battle WHERE battle_id = " + std::to_string(id));
        }
        for (auto id : player_ids) {
            mysql.Execute("DELETE FROM player WHERE player_id = " + std::to_string(id));
        }
        if (rank_touched) {
            const auto config = game::ConfigManager::Instance().Redis();
            auto* redis = redisConnect(config.host.c_str(), config.port);
            if (redis && !redis->err) {
                for (auto id : player_ids) {
                    auto* reply = redisCommand(redis, "ZREM rank:score %s", std::to_string(id).c_str());
                    if (reply) {
                        freeReplyObject(reply);
                    }
                }
            }
            if (redis) {
                redisFree(redis);
            }
        }
    }

    int64_t AddPlayer() {
        game::PlayerData data;
        data.player_id = UniqueId();
        data.name = "settlement_test";
        player_ids.push_back(data.player_id);
        EXPECT_TRUE(players.CreatePlayer(data));
        return data.player_id;
    }

    int64_t AddBattle() {
        const auto id = battles.CreateBattle(room_id);
        if (id > 0) {
            battle_ids.push_back(id);
        }
        return id;
    }

    int64_t Count(const std::string& table) {
        const auto rows = mysql.Query("SELECT COUNT(*) AS n FROM " + table +
                                      " WHERE battle_id = " + std::to_string(battle_id));
        EXPECT_EQ(rows.size(), 1u);
        return rows.empty() ? -1 : std::stoll(rows[0].at("n"));
    }

    void ExpectPlayer(int64_t id, int score, int wins, int losses) {
        game::PlayerData data;
        ASSERT_TRUE(players.FindPlayer(id, data));
        EXPECT_EQ(data.score, score);
        EXPECT_EQ(data.win_count, wins);
        EXPECT_EQ(data.lose_count, losses);
    }

    void ExpectRankScore(game::RankService& rank, int64_t id, int score) {
        for (const auto& item : rank.GetTopN(0, 100000)) {
            if (item.player_id == id) {
                EXPECT_EQ(item.score, score);
                return;
            }
        }
        FAIL() << "player missing from rank: " << id;
    }

    game::MysqlClient mysql;
    game::BattleRepository battles{&mysql};
    game::PlayerRepository players{&mysql};
    game::SettlementService service{&battles, &players, nullptr};
    int64_t room_id = 0;
    int64_t battle_id = 0;
    int64_t winner_id = 0;
    int64_t loser_id = 0;
    std::vector<int64_t> battle_ids;
    std::vector<int64_t> player_ids;
    bool rank_touched = false;
};

TEST_F(SettlementTest, LogAndPlayerResultAreUniqueByBattlePlayer) {
    game::SettlementLog log;
    log.battle_id = battle_id;
    log.player_id = winner_id;
    log.score_delta = 20;
    EXPECT_TRUE(battles.InsertSettlementLog(log));
    // 两次都由数据库生成不同的 settlement_id，真正验证复合唯一键。
    EXPECT_FALSE(battles.InsertSettlementLog(log));

    game::BattlePlayerResult result{battle_id, winner_id, "WIN", 20};
    EXPECT_TRUE(battles.InsertBattlePlayerResult(result));
    EXPECT_FALSE(battles.InsertBattlePlayerResult(result));
    EXPECT_EQ(Count("settlement_log"), 1);
    EXPECT_EQ(Count("battle_player_result"), 1);
}

TEST_F(SettlementTest, RepeatedSettlementUpdatesScoresAndStatsOnce) {
    const auto another_loser = AddPlayer();
    ASSERT_TRUE(service.SettleBattle(battle_id, room_id, winner_id, {loser_id, another_loser}));
    for (int i = 0; i < 3; ++i) {
        ASSERT_TRUE(service.SettleBattle(battle_id, room_id, winner_id, {another_loser, loser_id, loser_id}));
    }
    ExpectPlayer(winner_id, 1020, 1, 0);
    ExpectPlayer(loser_id, 990, 0, 1);
    ExpectPlayer(another_loser, 990, 0, 1);
    EXPECT_EQ(Count("battle"), 1);
    EXPECT_EQ(Count("battle_player_result"), 3);
    EXPECT_EQ(Count("settlement_log"), 3);
}

TEST_F(SettlementTest, ConcurrentConnectionsSettleOnlyOnce) {
    std::promise<void> start;
    auto ready = start.get_future().share();
    std::vector<std::future<bool>> workers;
    for (int i = 0; i < 4; ++i) {
        workers.push_back(std::async(std::launch::async, [&, ready]() {
            game::MysqlClient connection;
            if (!connection.Connect()) {
                return false;
            }
            game::BattleRepository local_battles(&connection);
            game::PlayerRepository local_players(&connection);
            game::SettlementService local_service(&local_battles, &local_players, nullptr);
            ready.wait();
            for (int j = 0; j < 3; ++j) {
                if (!local_service.SettleBattle(battle_id, room_id, winner_id, {loser_id})) {
                    return false;
                }
            }
            return true;
        }));
    }
    start.set_value();
    for (auto& worker : workers) {
        EXPECT_TRUE(worker.get());
    }
    ExpectPlayer(winner_id, 1020, 1, 0);
    ExpectPlayer(loser_id, 990, 0, 1);
    EXPECT_EQ(Count("battle_player_result"), 2);
    EXPECT_EQ(Count("settlement_log"), 2);
}

TEST_F(SettlementTest, FailureRollsBackAllPlayersAndRetrySucceeds) {
    ASSERT_TRUE(mysql.Execute("UPDATE player SET score = " + std::to_string(std::numeric_limits<int>::min()) +
                              " WHERE player_id = " + std::to_string(loser_id)));
    // winner 先更新，loser 减分时溢出；整局必须回滚，不能留下 winner 的积分或防重日志。
    ASSERT_FALSE(service.SettleBattle(battle_id, room_id, winner_id, {loser_id}));
    ExpectPlayer(winner_id, 1000, 0, 0);
    ExpectPlayer(loser_id, std::numeric_limits<int>::min(), 0, 0);
    EXPECT_EQ(Count("battle_player_result"), 0);
    EXPECT_EQ(Count("settlement_log"), 0);
    const auto rows = mysql.Query("SELECT settled, winner_id FROM battle WHERE battle_id = " +
                                  std::to_string(battle_id));
    ASSERT_EQ(rows.size(), 1u);
    EXPECT_EQ(rows[0].at("settled"), "0");
    EXPECT_EQ(rows[0].at("winner_id"), "0");

    ASSERT_TRUE(mysql.Execute("UPDATE player SET score = 1000 WHERE player_id = " + std::to_string(loser_id)));
    ASSERT_TRUE(service.SettleBattle(battle_id, room_id, winner_id, {loser_id}));
    ASSERT_TRUE(service.SettleBattle(battle_id, room_id, winner_id, {loser_id}));
    ExpectPlayer(winner_id, 1020, 1, 0);
    ExpectPlayer(loser_id, 990, 0, 1);
    EXPECT_EQ(Count("settlement_log"), 2);
}

TEST_F(SettlementTest, ConflictingReplayIsRejected) {
    ASSERT_TRUE(service.SettleBattle(battle_id, room_id, winner_id, {loser_id}));
    EXPECT_FALSE(service.SettleBattle(battle_id, room_id, loser_id, {winner_id}));
    EXPECT_FALSE(service.SettleBattle(battle_id, room_id + 1, winner_id, {loser_id}));
    EXPECT_FALSE(service.SettleBattle(battle_id, room_id, winner_id, {}));
    ExpectPlayer(winner_id, 1020, 1, 0);
    ExpectPlayer(loser_id, 990, 0, 1);
    EXPECT_EQ(Count("settlement_log"), 2);
}

TEST_F(SettlementTest, NewBattleInSameRoomUsesDifferentId) {
    ASSERT_TRUE(service.SettleBattle(battle_id, room_id, winner_id, {loser_id}));
    mysql.Disconnect();
    ASSERT_TRUE(mysql.Connect());
    const auto next_battle = AddBattle();
    ASSERT_GT(next_battle, battle_id);
    ASSERT_TRUE(service.SettleBattle(next_battle, room_id, winner_id, {loser_id}));
    ExpectPlayer(winner_id, 1040, 2, 0);
    ExpectPlayer(loser_id, 980, 0, 2);
}

TEST_F(SettlementTest, LostConnectionBeforeIdAllocationCanRetry) {
    const auto rows = mysql.Query("SELECT CONNECTION_ID() AS id");
    ASSERT_EQ(rows.size(), 1u);
    game::MysqlClient killer;
    ASSERT_TRUE(killer.Connect());
    ASSERT_TRUE(killer.Execute("KILL CONNECTION " + rows[0].at("id")));

    EXPECT_EQ(battles.CreateBattle(room_id), 0);
    const auto next_battle = AddBattle();
    ASSERT_GT(next_battle, 0);
    ASSERT_TRUE(service.SettleBattle(next_battle, room_id, winner_id, {loser_id}));
    ExpectPlayer(winner_id, 1020, 1, 0);
    ExpectPlayer(loser_id, 990, 0, 1);
}

TEST_F(SettlementTest, TransactionExceptionRollsBackAndReleasesConnection) {
    EXPECT_THROW(mysql.RunTransaction([&]() -> bool {
        EXPECT_TRUE(players.UpdatePlayerScore(winner_id, 20));
        throw std::runtime_error("injected transaction failure");
    }), std::runtime_error);
    ExpectPlayer(winner_id, 1000, 0, 0);
    ASSERT_TRUE(service.SettleBattle(battle_id, room_id, winner_id, {loser_id}));
    ExpectPlayer(winner_id, 1020, 1, 0);
}

TEST_F(SettlementTest, RankFailureCanRetryWithoutDoubleSettlement) {
    game::RedisClient redis;
    if (!redis.Connect()) {
        GTEST_SKIP() << "Redis is not available";
    }
    game::RankService unavailable_rank;
    game::SettlementService unavailable(&battles, &players, &unavailable_rank);
    ASSERT_FALSE(unavailable.SettleBattle(battle_id, room_id, winner_id, {loser_id}));
    ExpectPlayer(winner_id, 1020, 1, 0);
    ExpectPlayer(loser_id, 990, 0, 1);

    game::RankService rank(&redis);
    game::SettlementService recovered(&battles, &players, &rank);
    rank_touched = true;
    ASSERT_TRUE(recovered.SettleBattle(battle_id, room_id, winner_id, {loser_id}));
    ExpectRankScore(rank, winner_id, 1020);
    ExpectRankScore(rank, loser_id, 990);
    ExpectPlayer(winner_id, 1020, 1, 0);
    EXPECT_EQ(Count("settlement_log"), 2);
}

TEST_F(SettlementTest, OldReplayRefreshesRankFromLatestDatabaseScore) {
    game::RedisClient redis;
    if (!redis.Connect()) {
        GTEST_SKIP() << "Redis is not available";
    }
    ASSERT_TRUE(service.SettleBattle(battle_id, room_id, winner_id, {loser_id}));
    const auto next_battle = AddBattle();
    ASSERT_GT(next_battle, 0);
    ASSERT_TRUE(service.SettleBattle(next_battle, room_id, loser_id, {winner_id}));

    game::RankService rank(&redis);
    game::SettlementService replay(&battles, &players, &rank);
    rank_touched = true;
    ASSERT_TRUE(replay.SettleBattle(battle_id, room_id, winner_id, {loser_id}));
    ExpectRankScore(rank, winner_id, 1010);
    ExpectRankScore(rank, loser_id, 1010);
    ExpectPlayer(winner_id, 1010, 1, 1);
    ExpectPlayer(loser_id, 1010, 1, 1);
}

} // namespace
