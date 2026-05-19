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

/** @brief Session 共享指针类型。 */
using SessionPtr = std::shared_ptr<Session>;

/**
 * @brief 单个客户端连接对应的会话对象。
 * @help Session 保存玩家身份、房间 ID、心跳时间，并通过弱引用连接对象发送消息。
 */
class Session : public std::enable_shared_from_this<Session> {
public:
    /**
     * @brief 创建会话。
     * @param session_id 会话 ID。
     * @param conn 对应 TCP 连接的弱引用。
     */
    Session(uint64_t session_id, std::weak_ptr<TcpConnection> conn);

    /**
     * @brief 发送 protobuf 消息。
     * @param msg_id 消息 ID。
     * @param msg protobuf 消息体。
     */
    void Send(uint32_t msg_id, const google::protobuf::Message& msg);

    /**
     * @brief 发送已构建好的 Packet。
     * @param packet 网络包。
     */
    void SendPacket(const Packet& packet);

    /**
     * @brief 关闭当前会话对应的连接。
     */
    void Close();

    /**
     * @brief 获取会话 ID。
     * @return session_id。
     */
    uint64_t SessionId() const;

    /**
     * @brief 绑定玩家 ID。
     * @param player_id 玩家 ID。
     */
    void BindPlayerId(int64_t player_id);

    /**
     * @brief 获取玩家 ID。
     * @return 当前绑定的玩家 ID，未登录时为 0。
     */
    int64_t PlayerId() const;

    /**
     * @brief 设置当前房间 ID。
     * @param room_id 房间 ID。
     */
    void SetRoomId(int64_t room_id);

    /**
     * @brief 获取当前房间 ID。
     * @return 房间 ID，未进房时为 0。
     */
    int64_t RoomId() const;

    /**
     * @brief 更新最近心跳时间为当前时间。
     */
    void UpdateHeartbeatTime();

    /**
     * @brief 获取最近心跳时间。
     * @return 毫秒时间戳。
     */
    int64_t LastHeartbeatMs() const;

private:
    uint64_t session_id_;
    int64_t player_id_ = 0;
    int64_t room_id_ = 0;
    std::weak_ptr<TcpConnection> conn_;
    int64_t last_heartbeat_ms_ = 0;
};

} // namespace game
