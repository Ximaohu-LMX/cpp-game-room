#include "room/room_manager.h"

#include "game/input_buffer.h"
#include "protocol/message_id.h"
#include "protocol/proto_helper.h"
#include "server/message_dispatcher.h"
#include "server/service_context.h"
#include "util/id_generator.h"
#include "util/logger.h"
#include "util/time_util.h"

#include "game.pb.h"
#include "room.pb.h"

namespace game {

RoomManager::RoomManager(ServiceContext* context) : context_(context) {}

void RoomManager::RegisterHandlers(MessageDispatcher& dispatcher) {
    dispatcher.RegisterHandler(MSG_READY_REQ, [this](const SessionPtr& session, const Packet& packet) {
        HandleReady(session, packet);
    });
    dispatcher.RegisterHandler(MSG_INPUT_REQ, [this](const SessionPtr& session, const Packet& packet) {
        HandleInput(session, packet);
    });
}

RoomPtr RoomManager::CreateRoom(const std::vector<int64_t>& player_ids) {
    const int64_t room_id = IdGenerator::NextRoomId();
    auto room = std::make_shared<Room>(room_id, context_ ? context_->connection_manager : nullptr);
    for (auto player_id : player_ids) {
        room->AddPlayer(player_id);
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        rooms_[room_id] = room;
        for (auto player_id : player_ids) {
            player_room_map_[player_id] = room_id;
            if (context_ && context_->connection_manager) {
                if (auto session = context_->connection_manager->GetByPlayerId(player_id)) {
                    session->SetRoomId(room_id);
                }
            }
        }
    }
    LOG_INFO("created room {}", room_id);
    return room;
}

RoomPtr RoomManager::GetRoom(int64_t room_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = rooms_.find(room_id);
    return it == rooms_.end() ? nullptr : it->second;
}

void RoomManager::RemoveRoom(int64_t room_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = rooms_.find(room_id);
    if (it != rooms_.end()) {
        for (auto player_id : it->second->PlayerIds()) {
            player_room_map_.erase(player_id);
        }
    }
    rooms_.erase(room_id);
    if (context_ && context_->game_loop) {
        context_->game_loop->RemoveRoom(room_id);
    }
}

RoomPtr RoomManager::GetPlayerRoom(int64_t player_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = player_room_map_.find(player_id);
    if (it == player_room_map_.end()) {
        return nullptr;
    }
    auto room_it = rooms_.find(it->second);
    return room_it == rooms_.end() ? nullptr : room_it->second;
}

void RoomManager::HandleReady(const SessionPtr& session, const Packet& packet) {
    if (!session) {
        return;
    }
    proto::ReadyRequest request;
    if (!ProtoHelper::Parse(packet, &request)) {
        return;
    }

    auto room = GetPlayerRoom(session->PlayerId());
    if (!room) {
        return;
    }
    if (room->SetReadyAndTryStart(session->PlayerId(), request.ready())) {
        StartGameRoom(room);
    }
}

void RoomManager::HandleInput(const SessionPtr& session, const Packet& packet) {
    if (!session || !context_ || !context_->game_loop) {
        return;
    }
    proto::InputRequest request;
    if (!ProtoHelper::Parse(packet, &request)) {
        return;
    }
    auto game_room = context_->game_loop->GetRoom(session->RoomId());
    if (!game_room) {
        return;
    }

    InputCommand input;
    input.player_id = request.input().player_id() == 0 ? session->PlayerId() : request.input().player_id();
    input.input_seq = request.input().input_seq();
    input.move_x = request.input().move_x();
    input.move_y = request.input().move_y();
    input.fire = request.input().fire();
    input.timestamp_ms = NowMs();
    game_room->HandleInput(input);
}

void RoomManager::StartGameRoom(const RoomPtr& room) {
    if (!room || !context_ || !context_->game_loop) {
        return;
    }
    auto game_room = std::make_shared<GameRoom>(room->RoomId());
    game_room->Start(room->PlayerIds());
    context_->game_loop->AddRoom(game_room);

    proto::RoomStateNotify notify;
    notify.set_room_id(room->RoomId());
    notify.set_state(static_cast<int32_t>(RoomState::Playing));
    for (auto player_id : room->PlayerIds()) {
        notify.add_player_ids(player_id);
    }
    room->Broadcast(MSG_ROOM_STATE_NOTIFY, notify);
    LOG_INFO("room {} started game", room->RoomId());
}

} // namespace game
