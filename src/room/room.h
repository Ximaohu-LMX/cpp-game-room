#pragma once

#include "net/connection_manager.h"
#include "room/room_player.h"
#include "room/room_state.h"

#include <google/protobuf/message.h>

#include <cstdint>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace game {

class Room {
public:
    Room(int64_t room_id, ConnectionManager* connection_manager);

    bool AddPlayer(int64_t player_id);
    bool RemovePlayer(int64_t player_id);

    void SetReady(int64_t player_id, bool ready);
    bool CanStart() const;
    void StartGame();
    void SetSettlement();
    void Close();

    void OnPlayerDisconnect(int64_t player_id);
    void OnPlayerReconnect(int64_t player_id, SessionPtr session);

    void Broadcast(uint32_t msg_id, const google::protobuf::Message& msg);

    RoomState State() const;
    int64_t RoomId() const;
    std::vector<int64_t> PlayerIds() const;

private:
    int64_t room_id_;
    ConnectionManager* connection_manager_;
    mutable std::mutex mutex_;
    RoomState state_ = RoomState::Waiting;
    std::unordered_map<int64_t, RoomPlayer> players_;
};

using RoomPtr = std::shared_ptr<Room>;

} // namespace game

