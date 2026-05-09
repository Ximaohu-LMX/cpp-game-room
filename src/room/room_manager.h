#pragma once

#include "game/game_loop.h"
#include "room/room.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace game {

class MessageDispatcher;
class ServiceContext;

class RoomManager {
public:
    explicit RoomManager(ServiceContext* context);

    void RegisterHandlers(MessageDispatcher& dispatcher);

    RoomPtr CreateRoom(const std::vector<int64_t>& player_ids);
    RoomPtr GetRoom(int64_t room_id);
    void RemoveRoom(int64_t room_id);

    RoomPtr GetPlayerRoom(int64_t player_id);

private:
    void HandleReady(const SessionPtr& session, const Packet& packet);
    void HandleInput(const SessionPtr& session, const Packet& packet);
    void StartGameRoom(const RoomPtr& room);

    ServiceContext* context_;
    std::mutex mutex_;
    std::unordered_map<int64_t, RoomPtr> rooms_;
    std::unordered_map<int64_t, int64_t> player_room_map_;
};

} // namespace game

