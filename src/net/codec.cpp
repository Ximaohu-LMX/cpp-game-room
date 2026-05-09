#include "net/codec.h"

#include <stdexcept>

namespace game {

std::vector<Packet> Codec::Decode(Buffer& buffer) {
    std::vector<Packet> packets;

    while (buffer.size() >= kPacketHeaderSize) {
        const uint32_t length = ReadUint32BE(buffer.data());
        if (length < kPacketMetaSize || length > kMaxPacketLength) {
            throw std::runtime_error("invalid packet length");
        }

        const size_t full_size = sizeof(uint32_t) + length;
        if (buffer.size() < full_size) {
            break;
        }

        Packet packet;
        packet.length = length;
        packet.msg_id = ReadUint32BE(buffer.data() + 4);
        packet.seq = ReadUint32BE(buffer.data() + 8);
        const size_t body_size = length - kPacketMetaSize;
        packet.body.assign(buffer.data() + kPacketHeaderSize, body_size);
        packets.push_back(std::move(packet));

        buffer.erase(buffer.begin(), buffer.begin() + static_cast<std::ptrdiff_t>(full_size));
    }

    return packets;
}

std::string Codec::Encode(const Packet& packet) const {
    if (packet.body.size() > kMaxPacketLength - kPacketMetaSize) {
        throw std::runtime_error("packet body too large");
    }

    const uint32_t length = static_cast<uint32_t>(kPacketMetaSize + packet.body.size());
    std::string out;
    out.reserve(sizeof(uint32_t) + length);
    AppendUint32BE(out, length);
    AppendUint32BE(out, packet.msg_id);
    AppendUint32BE(out, packet.seq);
    out.append(packet.body);
    return out;
}

} // namespace game

