#include "net/tcp_connection.h"

#include "util/logger.h"

#include <utility>

namespace game {

TcpConnection::TcpConnection(boost::asio::io_context& io_context, uint64_t conn_id)
    : conn_id_(conn_id), strand_(boost::asio::make_strand(io_context.get_executor())), socket_(strand_) {}

TcpConnection::tcp::socket& TcpConnection::Socket() {
    return socket_;
}

void TcpConnection::SetSession(SessionPtr session) {
    session_ = std::move(session);
}

void TcpConnection::SetMessageCallback(MessageCallback callback) {
    message_callback_ = std::move(callback);
}

void TcpConnection::SetCloseCallback(CloseCallback callback) {
    close_callback_ = std::move(callback);
}

void TcpConnection::Start() {
    DoRead();
}

void TcpConnection::Send(const Packet& packet) {
    auto self = shared_from_this();
    boost::asio::post(socket_.get_executor(), [this, self, packet]() {
        const bool writing = !write_queue_.empty();
        write_queue_.push_back(codec_.Encode(packet));
        if (!writing) {
            DoWrite();
        }
    });
}

void TcpConnection::Close() {
    auto self = shared_from_this();
    boost::asio::post(socket_.get_executor(), [this, self]() {
        boost::system::error_code ignored;
        socket_.shutdown(tcp::socket::shutdown_both, ignored);
        socket_.close(ignored);
        if (close_callback_) {
            close_callback_(conn_id_);
        }
    });
}

uint64_t TcpConnection::ConnId() const {
    return conn_id_;
}

void TcpConnection::DoRead() {
    auto self = shared_from_this();
    socket_.async_read_some(
        boost::asio::buffer(read_temp_),
        [this, self](const boost::system::error_code& ec, std::size_t bytes) {
            if (ec) {
                LOG_WARN("connection {} read failed: {}", conn_id_, ec.message());
                if (close_callback_) {
                    close_callback_(conn_id_);
                }
                return;
            }

            read_buffer_.insert(read_buffer_.end(), read_temp_.data(), read_temp_.data() + bytes);
            try {
                auto packets = codec_.Decode(read_buffer_);
                for (const auto& packet : packets) {
                    if (message_callback_) {
                        message_callback_(session_, packet);
                    }
                }
            } catch (const std::exception& e) {
                LOG_ERROR("decode packet failed: {}", e.what());
                Close();
                return;
            }

            DoRead();
        });
}

void TcpConnection::DoWrite() {
    if (write_queue_.empty()) {
        return;
    }

    auto self = shared_from_this();
    boost::asio::async_write(
        socket_,
        boost::asio::buffer(write_queue_.front()),
        [this, self](const boost::system::error_code& ec, std::size_t) {
            if (ec) {
                LOG_WARN("connection {} write failed: {}", conn_id_, ec.message());
                if (close_callback_) {
                    close_callback_(conn_id_);
                }
                return;
            }

            write_queue_.pop_front();
            if (!write_queue_.empty()) {
                DoWrite();
            }
        });
}

} // namespace game

