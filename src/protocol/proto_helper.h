#pragma once

#include "net/packet.h"

#include <google/protobuf/message.h>

namespace game {

/**
 * @brief protobuf 与 Packet 互转工具。
 */
class ProtoHelper {
public:
    /**
     * @brief 从 Packet body 解析 protobuf 消息。
     * @param packet 网络包。
     * @param msg 输出 protobuf 消息指针。
     * @return 解析成功返回 true，否则返回 false。
     */
    template <typename T>
    static bool Parse(const Packet& packet, T* msg) {
        return msg != nullptr && msg->ParseFromString(packet.body);
    }

    /**
     * @brief 将 protobuf 消息构造成 Packet。
     * @param msg_id 消息 ID。
     * @param msg protobuf 消息。
     * @param seq 请求序号。
     * @return 构造后的 Packet。
     */
    static Packet Build(uint32_t msg_id, const google::protobuf::Message& msg, uint32_t seq = 0);
};

} // namespace game
