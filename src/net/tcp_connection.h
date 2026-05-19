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

/**
 * @brief 单个 TCP 连接封装。
 * @help 使用 per-connection strand 串行化同一连接上的异步读写和关闭回调。
 */
class TcpConnection : public std::enable_shared_from_this<TcpConnection> {
public:
    using tcp = boost::asio::ip::tcp;

    /**
     * @brief 收到完整 Packet 后触发的回调。
     * @param session 当前连接会话。
     * @param packet 解码后的网络包。
     */
    using MessageCallback = std::function<void(const SessionPtr&, const Packet&)>;

    /**
     * @brief 连接关闭回调。
     * @param conn_id 连接 ID。
     */
    using CloseCallback = std::function<void(uint64_t)>;

    /**
     * @brief 创建 TCP 连接。
     * @param io_context Asio 事件循环。
     * @param conn_id 连接 ID。
     */
    TcpConnection(boost::asio::io_context& io_context, uint64_t conn_id);

    /**
     * @brief 获取底层 socket，供 acceptor 接收连接。
     * @return TCP socket 引用。
     */
    tcp::socket& Socket();

    /**
     * @brief 设置连接对应的 Session。
     * @param session 会话对象。
     */
    void SetSession(SessionPtr session);

    /**
     * @brief 设置消息回调。
     * @param callback 收到完整包后的回调。
     */
    void SetMessageCallback(MessageCallback callback);

    /**
     * @brief 设置关闭回调。
     * @param callback 连接关闭后的回调。
     */
    void SetCloseCallback(CloseCallback callback);

    /**
     * @brief 开始异步读取。
     */
    void Start();

    /**
     * @brief 异步发送 Packet。
     * @param packet 待发送网络包。
     */
    void Send(const Packet& packet);

    /**
     * @brief 主动关闭连接。
     */
    void Close();

    /**
     * @brief 获取连接 ID。
     * @return conn_id。
     */
    uint64_t ConnId() const;

private:
    /**
     * @brief 投递一次异步读操作。
     */
    void DoRead();

    /**
     * @brief 投递一次异步写操作。
     */
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

/** @brief TcpConnection 共享指针类型。 */
using TcpConnectionPtr = std::shared_ptr<TcpConnection>;

} // namespace game
