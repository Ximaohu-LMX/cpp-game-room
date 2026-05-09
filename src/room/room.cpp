#include "room/room.h"

#include "protocol/message_id.h"

#include "room.pb.h"

namespace game {

Room::Room(int64_t room_id, ConnectionManager* connection_manager)
    : room_id_(room_id), connection_manager_(connection_manager) {}

bool Room::AddPlayer(int64_t player_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (state_ != RoomState::Waiting) {
        return false;
    }
    RoomPlayer player;
    player.player_id = player_id;
    players_[player_id] = player;
    return true;
}

bool Room::RemovePlayer(int64_t player_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    return players_.erase(player_id) > 0;
}

void Room::SetReady(int64_t player_id, bool ready) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = players_.find(player_id);
        if (it == players_.end() || state_ == RoomState::Playing || state_ == RoomState::Closed) {
            return;
        }
        it->second.ready = ready;
        bool all_ready = !players_.empty();
        for (const auto& [_, player] : players_) {
            all_ready = all_ready && player.ready;
        }
        state_ = all_ready ? RoomState::Ready : RoomState::Waiting;
    }

    proto::ReadyNotify notify;
    notify.set_player_id(player_id);
    notify.set_ready(ready);
    Broadcast(MSG_READY_NOTIFY, notify);
}

bool Room::CanStart() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return state_ == RoomState::Ready;
}

void Room::StartGame() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (state_ == RoomState::Ready) {
        state_ = RoomState::Playing;
    }
}

void Room::SetSettlement() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (state_ == RoomState::Playing) {
        state_ = RoomState::Settlement;
    }
}

void Room::Close() {
    std::lock_guard<std::mutex> lock(mutex_);
    state_ = RoomState::Closed;
}

void Room::OnPlayerDisconnect(int64_t player_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = players_.find(player_id);
    if (it != players_.end()) {
        it->second.connected = false;
    }
}

void Room::OnPlayerReconnect(int64_t player_id, SessionPtr session) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = players_.find(player_id);
        if (it != players_.end()) {
            it->second.connected = true;
        }
    }
    if (session) {
        session->SetRoomId(room_id_);
    }
}

void Room::Broadcast(uint32_t msg_id, const google::protobuf::Message& msg) {
    auto player_ids = PlayerIds();
    if (!connection_manager_) {
        return;
    }
    for (auto player_id : player_ids) {
        if (auto session = connection_manager_->GetByPlayerId(player_id)) {
            session->Send(msg_id, msg);
        }
    }
}

RoomState Room::State() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return state_;
}

int64_t Room::RoomId() const {
    return room_id_;
}

std::vector<int64_t> Room::PlayerIds() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<int64_t> ids;
    ids.reserve(players_.size());
    for (const auto& [player_id, _] : players_) {
        ids.push_back(player_id);
    }
    return ids;
}

} // namespace game
