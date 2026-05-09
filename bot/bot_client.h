#pragma once

#include "net/codec.h"
#include "net/packet.h"

#include <boost/asio.hpp>

#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <thread>

namespace game {

class BotClient {
public:
    BotClient(std::string host, uint16_t port, int index);
    ~BotClient();

    void Start();
    void Stop();

private:
    void DoConnect();
    void DoRead();
    void SendPacket(const Packet& packet);
    void Login();
    void Match();
    void Ready();
    void SendInput();
    void OnMessage(const Packet& packet);

    std::string host_;
    uint16_t port_;
    int index_;
    boost::asio::io_context io_context_;
    boost::asio::ip::tcp::socket socket_;
    std::thread worker_;
    std::atomic<bool> running_{false};

    Codec codec_;
    Buffer read_buffer_;
    std::array<char, 4096> read_temp_{};
    uint32_t seq_ = 0;
    int64_t player_id_ = 0;
    int64_t room_id_ = 0;
    int64_t input_seq_ = 0;
};

} // namespace game

