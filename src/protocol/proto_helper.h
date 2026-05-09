#pragma once

#include "net/packet.h"

#include <google/protobuf/message.h>

namespace game {

class ProtoHelper {
public:
    template <typename T>
    static bool Parse(const Packet& packet, T* msg) {
        return msg != nullptr && msg->ParseFromString(packet.body);
    }

    static Packet Build(uint32_t msg_id, const google::protobuf::Message& msg, uint32_t seq = 0);
};

} // namespace game

