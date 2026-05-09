#pragma once

#include <cstdint>
#include <string>

namespace game {

constexpr uint32_t kPacketHeaderSize = 12;
constexpr uint32_t kPacketMetaSize = 8;
constexpr uint32_t kMaxPacketLength = 1024 * 1024;

struct Packet {
    uint32_t length = kPacketMetaSize;
    uint32_t msg_id = 0;
    uint32_t seq = 0;
    std::string body;
};

uint32_t ReadUint32BE(const char* data);
void AppendUint32BE(std::string& out, uint32_t value);

} // namespace game

