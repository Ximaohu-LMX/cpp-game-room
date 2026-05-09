#include "protocol/proto_helper.h"

namespace game {

Packet ProtoHelper::Build(uint32_t msg_id, const google::protobuf::Message& msg, uint32_t seq) {
    Packet packet;
    packet.msg_id = msg_id;
    packet.seq = seq;
    msg.SerializeToString(&packet.body);
    packet.length = static_cast<uint32_t>(kPacketMetaSize + packet.body.size());
    return packet;
}

} // namespace game

