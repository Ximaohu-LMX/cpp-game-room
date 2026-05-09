#include "net/session.h"

#include "net/tcp_connection.h"
#include "protocol/proto_helper.h"
#include "util/time_util.h"

namespace game {

Session::Session(uint64_t session_id, std::weak_ptr<TcpConnection> conn)
    : session_id_(session_id), conn_(std::move(conn)), last_heartbeat_ms_(NowMs()) {}

void Session::Send(uint32_t msg_id, const google::protobuf::Message& msg) {
    SendPacket(ProtoHelper::Build(msg_id, msg));
}

void Session::SendPacket(const Packet& packet) {
    if (auto conn = conn_.lock()) {
        conn->Send(packet);
    }
}

void Session::Close() {
    if (auto conn = conn_.lock()) {
        conn->Close();
    }
}

uint64_t Session::SessionId() const {
    return session_id_;
}

void Session::BindPlayerId(int64_t player_id) {
    player_id_ = player_id;
}

int64_t Session::PlayerId() const {
    return player_id_;
}

void Session::SetRoomId(int64_t room_id) {
    room_id_ = room_id;
}

int64_t Session::RoomId() const {
    return room_id_;
}

void Session::UpdateHeartbeatTime() {
    last_heartbeat_ms_ = NowMs();
}

int64_t Session::LastHeartbeatMs() const {
    return last_heartbeat_ms_;
}

} // namespace game

