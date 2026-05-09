#include "match/match_queue.h"

#include <gtest/gtest.h>

TEST(MatchQueueTest, PreventDuplicatePush) {
    game::MatchQueue queue;
    EXPECT_TRUE(queue.Push(1));
    EXPECT_FALSE(queue.Push(1));
    EXPECT_EQ(queue.Size(), 1u);
}

TEST(MatchQueueTest, RemoveAndPopN) {
    game::MatchQueue queue;
    queue.Push(1);
    queue.Push(2);
    queue.Push(3);
    EXPECT_TRUE(queue.Remove(2));
    auto players = queue.PopN(2);
    ASSERT_EQ(players.size(), 2u);
    EXPECT_EQ(players[0], 1);
    EXPECT_EQ(players[1], 3);
}

