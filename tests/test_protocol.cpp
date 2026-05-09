#include "net/codec.h"

#include <gtest/gtest.h>

TEST(ProtocolTest, EncodeDecodeOnePacket) {
    game::Codec codec;
    game::Packet packet;
    packet.msg_id = 1001;
    packet.seq = 7;
    packet.body = "hello";

    auto bytes = codec.Encode(packet);
    game::Buffer buffer(bytes.begin(), bytes.end());
    auto packets = codec.Decode(buffer);

    ASSERT_EQ(packets.size(), 1u);
    EXPECT_EQ(packets[0].msg_id, 1001u);
    EXPECT_EQ(packets[0].seq, 7u);
    EXPECT_EQ(packets[0].body, "hello");
    EXPECT_TRUE(buffer.empty());
}

TEST(ProtocolTest, HalfPacketWaitsForMoreData) {
    game::Codec codec;
    game::Packet packet;
    packet.msg_id = 1;
    packet.body = "abcdef";
    auto bytes = codec.Encode(packet);

    game::Buffer buffer(bytes.begin(), bytes.begin() + 5);
    EXPECT_TRUE(codec.Decode(buffer).empty());
    buffer.insert(buffer.end(), bytes.begin() + 5, bytes.end());
    EXPECT_EQ(codec.Decode(buffer).size(), 1u);
}

