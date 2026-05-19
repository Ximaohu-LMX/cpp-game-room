#pragma once

#include <cstdint>
#include <string>

namespace game {

/** @brief 网络包头总长度：length + msg_id + seq。 */
constexpr uint32_t kPacketHeaderSize = 12;

/** @brief length 字段中固定元信息长度：msg_id + seq。 */
constexpr uint32_t kPacketMetaSize = 8;

/** @brief 单包最大长度，防止恶意 length 导致内存膨胀。 */
constexpr uint32_t kMaxPacketLength = 1024 * 1024;

/**
 * @brief 解码后的网络包。
 * @help length 表示 msg_id + seq + body 的长度，不包含 length 字段本身。
 */
struct Packet {
    uint32_t length = kPacketMetaSize; ///< 包长度。
    uint32_t msg_id = 0; ///< 消息 ID。
    uint32_t seq = 0; ///< 请求序号。
    std::string body; ///< protobuf 序列化后的消息体。
};

/**
 * @brief 从大端字节序读取 uint32。
 * @param data 指向 4 字节数据的指针。
 * @return 解析后的 uint32 值。
 */
uint32_t ReadUint32BE(const char* data);

/**
 * @brief 按大端字节序追加 uint32。
 * @param out 输出字符串。
 * @param value 待追加的数值。
 */
void AppendUint32BE(std::string& out, uint32_t value);

} // namespace game
