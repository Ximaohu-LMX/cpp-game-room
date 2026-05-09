#pragma once

#include "net/connection_manager.h"
#include "net/tcp_connection.h"

#include <boost/asio.hpp>

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace game {

class TcpServer {
public:
    using MessageCallback = TcpConnection::MessageCallback;

    TcpServer(ConnectionManager& connection_manager, MessageCallback message_callback, int worker_threads);
    ~TcpServer();

    bool Start(const std::string& host, uint16_t port);
    void Stop();

private:
    void DoAccept();

    boost::asio::io_context io_context_;
    boost::asio::ip::tcp::acceptor acceptor_;
    ConnectionManager& connection_manager_;
    MessageCallback message_callback_;
    int worker_threads_ = 1;
    std::atomic<bool> running_{false};
    std::vector<std::thread> workers_;
};

} // namespace game

