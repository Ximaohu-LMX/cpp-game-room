#include "config/config_manager.h"
#include "rank/rank_service.h"
#include "storage/redis_client.h"

#include <chrono>
#include <string>

#include <gtest/gtest.h>

namespace {

int64_t UniquePlayerId(int suffix) {
    const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    return 900000000000LL + (now % 1000000000LL) * 10 + suffix;
}

void LoadTestConfig() {
    game::ConfigManager::Instance().Load(std::string(GAME_TEST_SOURCE_DIR) + "/config/server.yaml");
}

} // namespace

TEST(RankServiceTest, GetTopNByScoreDesc) {
    LoadTestConfig();
    game::RedisClient redis;
    if (!redis.Connect()) {
        GTEST_SKIP() << "Redis is not available";
    }
    game::RankService rank(&redis);

    const auto p1 = UniquePlayerId(1);
    const auto p2 = UniquePlayerId(2);
    const auto p3 = UniquePlayerId(3);

    rank.UpdateScore(p1, 1999999998);
    rank.UpdateScore(p2, 2000000000);
    rank.UpdateScore(p3, 1999999999);

    auto items = rank.GetTopN(0, 3);

    ASSERT_EQ(items.size(), 3u);
    EXPECT_EQ(items[0].player_id, p2);
    EXPECT_EQ(items[0].score, 2000000000);
    EXPECT_EQ(items[0].rank, 1);

    EXPECT_EQ(items[1].player_id, p3);
    EXPECT_EQ(items[1].score, 1999999999);
    EXPECT_EQ(items[1].rank, 2);

    EXPECT_EQ(items[2].player_id, p1);
    EXPECT_EQ(items[2].score, 1999999998);
    EXPECT_EQ(items[2].rank, 3);
}
