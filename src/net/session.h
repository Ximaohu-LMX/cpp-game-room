#pragma once

#include "net/packet.h"

#include <cstdint>
#include <memory>

namespace google::protobuf {
class Message;
}

namespace game {

class TcpConnection;
class Session;

using SessionPtr = std::shared_ptr<Session>;

class Session : public std::enable_shared_from_this<Session> {
public:
    Session(uint64_t session_id, std::weak_ptr<TcpConnection> conn);

    void Send(uint32_t msg_id, const google::protobuf::Message& msg);
    void SendPacket(const Packet& packet);
    void Close();

    uint64_t SessionId() const;

    void BindPlayerId(int64_t player_id);
    int64_t PlayerId() const;

    void SetRoomId(int64_t room_id);
    int64_t RoomId() const;

    void UpdateHeartbeatTime();
    int64_t LastHeartbeatMs() const;

private:
    uint64_t session_id_;
    int64_t player_id_ = 0;
    int64_t room_id_ = 0;
    std::weak_ptr<TcpConnection> conn_;
    int64_t last_heartbeat_ms_ = 0;
};

} // namespace game

