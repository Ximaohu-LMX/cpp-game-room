#pragma once

#include "net/packet.h"

#include <string>
#include <vector>

namespace game {

/** @brief TCP 字节流缓冲区。 */
using Buffer = std::vector<char>;

/**
 * @brief 网络协议编解码器。
 * @help 负责 length + msg_id + seq + protobuf body 协议的粘包拆包与封包。
 */
class Codec {
public:
    /**
     * @brief 从缓冲区中解码出完整 Packet。
     * @param buffer TCP 累积缓冲区；成功解出的字节会被移除，半包保留。
     * @return 当前缓冲区中所有完整 Packet。
     */
    std::vector<Packet> Decode(Buffer& buffer);

    /**
     * @brief 将 Packet 编码成可发送的二进制字节串。
     * @param packet 待发送网络包。
     * @return 编码后的字节串。
     */
    std::string Encode(const Packet& packet) const;
};

} // namespace game
