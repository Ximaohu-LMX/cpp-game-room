#include "net/tcp_server.h"

#include "util/id_generator.h"
#include "util/logger.h"

#include <utility>

namespace game {

TcpServer::TcpServer(ConnectionManager& connection_manager, MessageCallback message_callback, int worker_threads)
    : acceptor_(io_context_),
      connection_manager_(connection_manager),
      message_callback_(std::move(message_callback)),
      worker_threads_(worker_threads > 0 ? worker_threads : 1) {}

TcpServer::~TcpServer() {
    Stop();
}

bool TcpServer::Start(const std::string& host, uint16_t port) {
    boost::system::error_code ec;
    boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::make_address(host, ec), port);
    if (ec) {
        LOG_ERROR("invalid listen address {}: {}", host, ec.message());
        return false;
    }

    acceptor_.open(endpoint.protocol(), ec);
    if (ec) {
        LOG_ERROR("open acceptor failed: {}", ec.message());
        return false;
    }

    acceptor_.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true), ec);
    acceptor_.bind(endpoint, ec);
    if (ec) {
        LOG_ERROR("bind failed: {}", ec.message());
        return false;
    }

    acceptor_.listen(boost::asio::socket_base::max_listen_connections, ec);
    if (ec) {
        LOG_ERROR("listen failed: {}", ec.message());
        return false;
    }

    running_ = true;
    DoAccept();

    LOG_INFO("tcp server listening on {}:{}", host, port);
    for (int i = 0; i < worker_threads_; ++i) {
        workers_.emplace_back([this]() { io_context_.run(); });
    }
    for (auto& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    return true;
}

void TcpServer::Stop() {
    if (!running_.exchange(false)) {
        return;
    }
    boost::system::error_code ignored;
    acceptor_.close(ignored);
    io_context_.stop();
}

void TcpServer::DoAccept() {
    auto conn_id = static_cast<uint64_t>(IdGenerator::NextSessionId());
    auto conn = std::make_shared<TcpConnection>(io_context_, conn_id);
    acceptor_.async_accept(conn->Socket(), [this, conn, conn_id](const boost::system::error_code& ec) {
        if (!ec) {
            auto session = std::make_shared<Session>(conn_id, conn);
            conn->SetSession(session);
            conn->SetMessageCallback(message_callback_);
            conn->SetCloseCallback([this](uint64_t id) { connection_manager_.Remove(id); });
            connection_manager_.Add(session);
            conn->Start();
            LOG_INFO("new connection {}", conn_id);
        } else {
            LOG_WARN("accept failed: {}", ec.message());
        }

        if (running_) {
            DoAccept();
        }
    });
}

} // namespace game
