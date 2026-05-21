#include "net/codec.h"

#include <stdexcept>

namespace game {

std::vector<Packet> Codec::Decode(Buffer& buffer) {
    // TODO: 练习目标：把 TCP 字节流缓冲区解码成 0 个或多个完整 Packet。
    //
    // 协议格式：
    //   | length 4 bytes | msg_id 4 bytes | seq 4 bytes | protobuf body |

    // 你需要返回解析出的 packets，并保留未完整到达的半包数据。


    std::vector<Packet> packets;

    while(buffer.size() >= kPacketHeaderSize) {

        uint32_t length = ReadUint32BE(buffer.data());

        if(length < kPacketMetaSize || length > kMaxPacketLength) {
            throw std::runtime_error("invalid packet length");
        }
        if(buffer.size() < length + sizeof(uint32_t)) {
            break;
        }
        uint32_t msg_id = ReadUint32BE(buffer.data() + 4);
        uint32_t seq = ReadUint32BE(buffer.data() + 8);
        std::string body(buffer.data() + 12, length - 8);
        packets.push_back(Packet{length, msg_id, seq, body});
        buffer.erase(buffer.begin(), buffer.begin() + 4 + length);
    }

    return packets;

}

std::string Codec::Encode(const Packet& packet) const {
    // TODO: 练习目标：把 Packet 编码成网络传输字节串。
    //
    // 协议格式：
    //   | length 4 bytes | msg_id 4 bytes | seq 4 bytes | protobuf body |
    //
    // 实现要求：
    //   1. 检查 body 大小，不能超过 kMaxPacketLength - kPacketMetaSize。
    //      超过时抛 std::runtime_error("packet body too large")。
    //   2. length = kPacketMetaSize + packet.body.size()。
    //      注意 length 表示 msg_id + seq + body 的长度，不包含 length 字段本身。
    //   3. 使用 AppendUint32BE 依次写入：
    //        - length
    //        - packet.msg_id
    //        - packet.seq
    //   4. 最后 append packet.body。
    //   5. 返回完整二进制字符串。

    std::string out;
    if(packet.body.size() > kMaxPacketLength - kPacketMetaSize) {
        throw std::runtime_error("packet body too large");
    }
    uint32_t length = kPacketMetaSize + packet.body.size();
    AppendUint32BE( out, length);
    AppendUint32BE( out, packet.msg_id);
    AppendUint32BE( out, packet.seq);
    out+=packet.body;
    return out;
}

} // namespace game
