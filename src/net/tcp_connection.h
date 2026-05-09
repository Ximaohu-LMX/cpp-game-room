#pragma once

#include "net/codec.h"
#include "net/session.h"

#include <boost/asio.hpp>

#include <array>
#include <deque>
#include <functional>
#include <memory>
#include <string>

namespace game {

class TcpConnection : public std::enable_shared_from_this<TcpConnection> {
public:
    using tcp = boost::asio::ip::tcp;
    using MessageCallback = std::function<void(const SessionPtr&, const Packet&)>;
    using CloseCallback = std::function<void(uint64_t)>;

    TcpConnection(boost::asio::io_context& io_context, uint64_t conn_id);

    tcp::socket& Socket();
    void SetSession(SessionPtr session);
    void SetMessageCallback(MessageCallback callback);
    void SetCloseCallback(CloseCallback callback);

    void Start();
    void Send(const Packet& packet);
    void Close();

    uint64_t ConnId() const;

private:
    void DoRead();
    void DoWrite();

    uint64_t conn_id_;
    boost::asio::strand<boost::asio::io_context::executor_type> strand_;
    tcp::socket socket_;
    std::array<char, 4096> read_temp_{};
    Buffer read_buffer_;
    std::deque<std::string> write_queue_;
    Codec codec_;
    SessionPtr session_;
    MessageCallback message_callback_;
    CloseCallback close_callback_;
};

using TcpConnectionPtr = std::shared_ptr<TcpConnection>;

} // namespace game

