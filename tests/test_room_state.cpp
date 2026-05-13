#include "room/room.h"

#include <gtest/gtest.h>

TEST(RoomStateTest, ReadyStartsGame) {
    game::Room room(1, nullptr);
    EXPECT_TRUE(room.AddPlayer(11));
    EXPECT_TRUE(room.AddPlayer(12));
    EXPECT_EQ(static_cast<int>(room.State()), static_cast<int>(game::RoomState::Waiting));

    room.SetReady(11, true);
    EXPECT_FALSE(room.CanStart());
    room.SetReady(12, true);
    EXPECT_TRUE(room.CanStart());
    room.StartGame();
    EXPECT_EQ(static_cast<int>(room.State()), static_cast<int>(game::RoomState::Playing));
}
