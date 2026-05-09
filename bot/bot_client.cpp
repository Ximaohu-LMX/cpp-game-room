#include "bot_client.h"

#include "protocol/message_id.h"
#include "protocol/proto_helper.h"
#include "util/random_util.h"

#include "game.pb.h"
#include "login.pb.h"
#include "match.pb.h"
#include "room.pb.h"

#include <chrono>
#include <utility>

namespace game {

BotClient::BotClient(std::string host, uint16_t port, int index)
    : host_(std::move(host)), port_(port), index_(index), socket_(io_context_) {}

BotClient::~BotClient() {
    Stop();
}

void BotClient::Start() {
    if (running_.exchange(true)) {
        return;
    }
    DoConnect();
    worker_ = std::thread([this]() { io_context_.run(); });
}

void BotClient::Stop() {
    if (!running_.exchange(false)) {
        return;
    }
    boost::system::error_code ignored;
    socket_.close(ignored);
    io_context_.stop();
    if (worker_.joinable()) {
        worker_.join();
    }
}

void BotClient::DoConnect() {
    auto endpoints = boost::asio::ip::tcp::resolver(io_context_).resolve(host_, std::to_string(port_));
    boost::asio::async_connect(socket_, endpoints, [this](const boost::system::error_code& ec, const auto&) {
        if (!ec) {
            Login();
            DoRead();
        }
    });
}

void BotClient::DoRead() {
    socket_.async_read_some(boost::asio::buffer(read_temp_), [this](const boost::system::error_code& ec, std::size_t bytes) {
        if (ec || !running_) {
            return;
        }
        read_buffer_.insert(read_buffer_.end(), read_temp_.data(), read_temp_.data() + bytes);
        try {
            auto packets = codec_.Decode(read_buffer_);
            for (const auto& packet : packets) {
                OnMessage(packet);
            }
        } catch (const std::exception&) {
            return;
        }
        DoRead();
    });
}

void BotClient::SendPacket(const Packet& packet) {
    auto data = std::make_shared<std::string>(codec_.Encode(packet));
    boost::asio::async_write(socket_, boost::asio::buffer(*data), [data](const boost::system::error_code&, std::size_t) {});
}

void BotClient::Login() {
    proto::LoginRequest request;
    request.set_account("bot_" + std::to_string(index_));
    request.set_token("bot_token");
    SendPacket(ProtoHelper::Build(MSG_LOGIN_REQ, request, ++seq_));
}

void BotClient::Match() {
    proto::MatchRequest request;
    request.set_player_id(player_id_);
    SendPacket(ProtoHelper::Build(MSG_MATCH_REQ, request, ++seq_));
}

void BotClient::Ready() {
    proto::ReadyRequest request;
    request.set_ready(true);
    SendPacket(ProtoHelper::Build(MSG_READY_REQ, request, ++seq_));
}

void BotClient::SendInput() {
    if (!running_ || room_id_ == 0) {
        return;
    }
    proto::InputRequest request;
    auto* input = request.mutable_input();
    input->set_player_id(player_id_);
    input->set_input_seq(++input_seq_);
    input->set_move_x(RandomUtil::Float(-1.0f, 1.0f));
    input->set_move_y(RandomUtil::Float(-1.0f, 1.0f));
    input->set_fire(RandomUtil::Int(0, 3) == 0);
    SendPacket(ProtoHelper::Build(MSG_INPUT_REQ, request, ++seq_));

    auto timer = std::make_shared<boost::asio::steady_timer>(io_context_, std::chrono::milliseconds(50));
    timer->async_wait([this, timer](const boost::system::error_code& ec) {
        if (!ec) {
            SendInput();
        }
    });
}

void BotClient::OnMessage(const Packet& packet) {
    if (packet.msg_id == MSG_LOGIN_RESP) {
        proto::LoginResponse response;
        if (ProtoHelper::Parse(packet, &response) && response.code() == 0) {
            player_id_ = response.player_id();
            Match();
        }
        return;
    }

    if (packet.msg_id == MSG_MATCH_SUCCESS_NOTIFY) {
        proto::MatchSuccessNotify notify;
        if (ProtoHelper::Parse(packet, &notify)) {
            room_id_ = notify.room_id();
            Ready();
            SendInput();
        }
        return;
    }

    if (packet.msg_id == MSG_GAME_OVER_NOTIFY) {
        room_id_ = 0;
        Match();
    }
}

} // namespace game
