#include "util/metrics.h"
#include "net/connection_manager.h"
#include "game/game_room.h"
#include <gtest/gtest.h>
#include <atomic>
#include <numeric>
#include <thread>
#include <vector>

TEST(MetricsTest, HistogramBoundsAndDrain) {
    game::Histogram h;
    EXPECT_EQ(h.Drain().count, 0u);
    for (uint64_t i = 1; i <= 1000; ++i) h.Observe(i);
    const auto data = h.Drain();
    EXPECT_EQ(data.count, 1000u);
    EXPECT_EQ(data.sum_us, 500500u);
    EXPECT_EQ(data.max_us, 1000u);
    EXPECT_GE(data.Percentile(0.95), 950);
    EXPECT_LE(data.Percentile(0.95), 1045);
    EXPECT_EQ(std::accumulate(data.buckets.begin(), data.buckets.end(), uint64_t{0}), data.count);
    EXPECT_EQ(h.Drain().count, 0u);
    h.Observe(1000000000000ULL);
    EXPECT_EQ(h.Drain().Percentile(0.99), 1000000000000ULL);
}

TEST(MetricsTest, ConcurrentExportLosesNoSamples) {
    game::Histogram h;
    std::atomic<int> done{0};
    std::vector<std::thread> writers;
    for (int i = 0; i < 4; ++i) writers.emplace_back([&]() {
        for (int j = 0; j < 10000; ++j) h.Observe(25);
        ++done;
    });
    uint64_t count = 0, sum = 0;
    while (done < 4) {
        const auto data = h.Drain();
        count += data.count; sum += data.sum_us;
        std::this_thread::yield();
    }
    for (auto& writer : writers) writer.join();
    const auto tail = h.Drain();
    EXPECT_EQ(count + tail.count, 40000u);
    EXPECT_EQ(sum + tail.sum_us, 1000000u);
}

TEST(MetricsTest, ConnectionGaugeCountsSessionsAndUniquePlayers) {
    game::ConnectionManager manager;
    auto a = std::make_shared<game::Session>(1, std::weak_ptr<game::TcpConnection>{});
    auto b = std::make_shared<game::Session>(2, std::weak_ptr<game::TcpConnection>{});
    manager.Add(a); manager.Add(b); manager.Add(a);
    a->BindPlayerId(11); b->BindPlayerId(11);
    manager.BindPlayer(11, a); manager.BindPlayer(11, b);
    EXPECT_EQ(manager.MeasurementGauges().at("connections"), 2);
    EXPECT_EQ(manager.MeasurementGauges().at("logged_in_players"), 1);
    manager.Remove(1); manager.Remove(1);
    EXPECT_EQ(manager.MeasurementGauges().at("connections"), 1);
    EXPECT_EQ(manager.MeasurementGauges().at("logged_in_players"), 1);
    manager.Remove(2);
    EXPECT_EQ(manager.MeasurementGauges().at("connections"), 0);
}

TEST(MetricsTest, CompletedBattleDurationRecordedOnlyOnce) {
    auto& metrics = game::Metrics::Instance();
    metrics.Enable(true);
    metrics.SnapshotJson({}, 0, 0);
    game::GameRoom room(1);
    room.Start({11, 12});
    room.EliminatePlayer(12);
    room.Tick(); room.Tick(); room.EliminatePlayer(12);
    const auto json = metrics.SnapshotJson({}, 1, 1);
    EXPECT_NE(json.find("\"battle_duration_us\":{\"count\":1,"), std::string::npos);
    const auto empty = metrics.SnapshotJson({}, 2, 1);
    EXPECT_NE(empty.find("\"battle_duration_us\":{\"count\":0,"), std::string::npos);
    metrics.Enable(false);
}
