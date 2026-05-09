#pragma once

#include "net/packet.h"

#include <string>
#include <vector>

namespace game {

using Buffer = std::vector<char>;

class Codec {
public:
    std::vector<Packet> Decode(Buffer& buffer);
    std::string Encode(const Packet& packet) const;
};

} // namespace game

