#include "room/room.h"

#include "game/game_loop.h"
#include "match/match_service.h"
#include "net/connection_manager.h"
#include "player/player_manager.h"
#include "protocol/message_id.h"
#include "protocol/proto_helper.h"
#include "room/room_manager.h"
#include "server/message_dispatcher.h"
#include "server/service_context.h"

#include "room.pb.h"

#include <gtest/gtest.h>

TEST(RoomStateTest, AllReadyStartsGame) {
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

TEST(RoomStateTest, PlayerStatusSyncsOnStartAndRemove) {
    game::ServiceContext context;
    game::ConnectionManager connection_manager;
    game::PlayerManager player_manager;
    game::GameLoop game_loop(50);
    game::RoomManager room_manager(&context);
    game::MessageDispatcher dispatcher;

    context.connection_manager = &connection_manager;
    context.player_manager = &player_manager;
    context.game_loop = &game_loop;
    context.room_manager = &room_manager;
    room_manager.RegisterHandlers(dispatcher);

    auto session1 = std::make_shared<game::Session>(1, std::weak_ptr<game::TcpConnection>{});
    auto session2 = std::make_shared<game::Session>(2, std::weak_ptr<game::TcpConnection>{});
    session1->BindPlayerId(11);
    session2->BindPlayerId(12);
    connection_manager.Add(session1);
    connection_manager.Add(session2);

    auto player1 = player_manager.GetOrCreatePlayer(11);
    auto player2 = player_manager.GetOrCreatePlayer(12);
    player_manager.SetOnline(11, session1);
    player_manager.SetOnline(12, session2);

    auto room = room_manager.CreateRoom({11, 12});
    player1->SetStatus(game::PlayerStatus::InRoom);
    player1->SetRoomId(room->RoomId());
    player2->SetStatus(game::PlayerStatus::InRoom);
    player2->SetRoomId(room->RoomId());

    proto::ReadyRequest ready;
    ready.set_ready(true);
    auto packet = game::ProtoHelper::Build(game::MSG_READY_REQ, ready);
    dispatcher.Dispatch(session1, packet);
    EXPECT_EQ(player1->Status(), game::PlayerStatus::InRoom);
    EXPECT_EQ(player2->Status(), game::PlayerStatus::InRoom);

    dispatcher.Dispatch(session2, packet);
    EXPECT_EQ(player1->Status(), game::PlayerStatus::Playing);
    EXPECT_EQ(player2->Status(), game::PlayerStatus::Playing);

    room_manager.RemoveRoom(room->RoomId());
    EXPECT_EQ(player1->Status(), game::PlayerStatus::Online);
    EXPECT_EQ(player1->RoomId(), 0);
    EXPECT_EQ(session1->RoomId(), 0);
    EXPECT_EQ(player2->Status(), game::PlayerStatus::Online);
    EXPECT_EQ(player2->RoomId(), 0);
    EXPECT_EQ(session2->RoomId(), 0);
}

TEST(RoomStateTest, LeaveWaitingRequeuesRemainingPlayers) {
    game::ServiceContext context;
    game::ConnectionManager connection_manager;
    game::PlayerManager player_manager;
    game::GameLoop game_loop(50);
    game::RoomManager room_manager(&context);
    game::MatchService match_service(&context);
    game::MessageDispatcher dispatcher;

    context.connection_manager = &connection_manager;
    context.player_manager = &player_manager;
    context.game_loop = &game_loop;
    context.room_manager = &room_manager;
    context.match_service = &match_service;
    room_manager.RegisterHandlers(dispatcher);

    auto session1 = std::make_shared<game::Session>(101, std::weak_ptr<game::TcpConnection>{});
    auto session2 = std::make_shared<game::Session>(102, std::weak_ptr<game::TcpConnection>{});
    session1->BindPlayerId(21);
    session2->BindPlayerId(22);
    connection_manager.Add(session1);
    connection_manager.Add(session2);
    player_manager.SetOnline(21, session1);
    player_manager.SetOnline(22, session2);

    auto player1 = player_manager.GetOrCreatePlayer(21);
    auto player2 = player_manager.GetOrCreatePlayer(22);
    auto room = room_manager.CreateRoom({21, 22});
    player1->SetStatus(game::PlayerStatus::InRoom);
    player1->SetRoomId(room->RoomId());
    player2->SetStatus(game::PlayerStatus::InRoom);
    player2->SetRoomId(room->RoomId());

    proto::LeaveRoomRequest leave;
    leave.set_room_id(room->RoomId());
    auto packet = game::ProtoHelper::Build(game::MSG_LEAVE_ROOM_REQ, leave);
    dispatcher.Dispatch(session1, packet);

    EXPECT_EQ(room_manager.GetPlayerRoom(21), nullptr);
    EXPECT_EQ(room_manager.GetPlayerRoom(22), nullptr);
    EXPECT_EQ(player1->Status(), game::PlayerStatus::Online);
    EXPECT_EQ(player1->RoomId(), 0);
    EXPECT_EQ(session1->RoomId(), 0);
    EXPECT_EQ(player2->Status(), game::PlayerStatus::Matching);
    EXPECT_EQ(player2->RoomId(), 0);
    EXPECT_EQ(session2->RoomId(), 0);
}
