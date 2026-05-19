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

/**
 * @brief TCP 服务端。
 * @help 负责监听端口、接收连接、创建 TcpConnection/Session，并用多线程运行 io_context。
 */
class TcpServer {
public:
    using MessageCallback = TcpConnection::MessageCallback;

    /**
     * @brief 创建 TCP 服务端。
     * @param connection_manager 连接管理器。
     * @param message_callback 收到完整 Packet 后的回调。
     * @param worker_threads io_context 工作线程数。
     */
    TcpServer(ConnectionManager& connection_manager, MessageCallback message_callback, int worker_threads);

    /**
     * @brief 析构时停止服务端。
     */
    ~TcpServer();

    /**
     * @brief 启动监听并运行 io_context。
     * @param host 监听地址。
     * @param port 监听端口。
     * @return 启动成功返回 true，绑定或监听失败返回 false。
     */
    bool Start(const std::string& host, uint16_t port);

    /**
     * @brief 停止监听并关闭 io_context。
     */
    void Stop();

private:
    /**
     * @brief 投递下一次异步 accept。
     */
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
