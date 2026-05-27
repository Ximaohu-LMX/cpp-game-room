#include "bot_client.h"

#include "protocol/message_id.h"
#include "protocol/proto_helper.h"
#include "util/random_util.h"
#include "util/time_util.h"

#include "game.pb.h"
#include "login.pb.h"
#include "match.pb.h"
#include "room.pb.h"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <utility>

namespace game {

BotClient::BotClient(std::string host, uint16_t port, int index, BotOptions options, std::shared_ptr<BotStats> stats)
    : host_(std::move(host)),
      port_(port),
      index_(index),
      options_(options),
      stats_(std::move(stats)) {}

BotClient::~BotClient() {
    Stop();
}

void BotClient::Start() {
    if (running_.exchange(true)) {
        return;
    }
    work_guard_ = std::make_unique<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>>(
        io_context_.get_executor());
    Connect(false);
    worker_ = std::thread([this]() { io_context_.run(); });
}

void BotClient::Stop() {
    if (!running_.exchange(false)) {
        return;
    }
    boost::asio::post(io_context_, [this]() { CloseSocket(); });
    if (work_guard_) {
        work_guard_->reset();
    }
    io_context_.stop();
    if (worker_.joinable()) {
        worker_.join();
    }
}

void BotClient::Connect(bool reconnect) {
    if (!running_) {
        return;
    }

    state_ = FlowState::Connecting;
    connected_ = false;
    read_buffer_.clear();
    write_queue_.clear();
    socket_ = std::make_shared<TcpSocket>(io_context_);

    boost::system::error_code resolve_ec;
    auto endpoints = boost::asio::ip::tcp::resolver(io_context_).resolve(host_, std::to_string(port_), resolve_ec);
    if (resolve_ec) {
        if (stats_) {
            ++stats_->connect_failed;
        }
        ScheduleTimer(options_.reconnect_delay_ms, [this, reconnect]() { Connect(reconnect); });
        return;
    }

    auto socket = socket_;
    boost::asio::async_connect(*socket, endpoints, [this, socket, reconnect](const boost::system::error_code& ec, const auto&) {
        if (!running_ || socket != socket_) {
            return;
        }
        if (ec) {
            if (stats_) {
                ++stats_->connect_failed;
            }
            ScheduleTimer(options_.reconnect_delay_ms, [this, reconnect]() { Connect(reconnect); });
            return;
        }

        connected_ = true;
        if (stats_) {
            ++stats_->connect_ok;
        }
        if (reconnect && player_id_ != 0 && !session_token_.empty()) {
            Reconnect();
        } else {
            Login();
        }
        DoRead(socket);
    });
}

void BotClient::ScheduleReconnect() {
    if (!running_ || player_id_ == 0 || session_token_.empty()) {
        return;
    }
    ScheduleTimer(JitterMs(options_.reconnect_delay_ms), [this]() { Connect(true); });
}

void BotClient::CloseSocket() {
    connected_ = false;
    input_loop_active_ = false;
    write_queue_.clear();

    auto socket = socket_;
    socket_.reset();
    if (!socket) {
        return;
    }

    boost::system::error_code ignored;
    socket->shutdown(TcpSocket::shutdown_both, ignored);
    socket->close(ignored);
}

void BotClient::DoRead(const std::shared_ptr<TcpSocket>& socket) {
    socket->async_read_some(boost::asio::buffer(read_temp_), [this, socket](const boost::system::error_code& ec, std::size_t bytes) {
        if (!running_ || socket != socket_) {
            return;
        }
        if (ec) {
            connected_ = false;
            return;
        }

        read_buffer_.insert(read_buffer_.end(), read_temp_.data(), read_temp_.data() + bytes);
        try {
            auto packets = codec_.Decode(read_buffer_);
            for (const auto& packet : packets) {
                OnMessage(packet);
            }
        } catch (const std::exception&) {
            CloseSocket();
            return;
        }
        DoRead(socket);
    });
}

void BotClient::SendPacket(const Packet& packet) {
    if (!running_) {
        return;
    }
    boost::asio::post(io_context_, [this, packet]() {
        if (!running_ || !connected_ || !socket_ || !socket_->is_open()) {
            return;
        }
        const bool writing = !write_queue_.empty();
        write_queue_.push_back(codec_.Encode(packet));
        if (!writing) {
            DoWrite(socket_);
        }
    });
}

void BotClient::DoWrite(const std::shared_ptr<TcpSocket>& socket) {
    if (!socket || socket != socket_ || write_queue_.empty()) {
        return;
    }
    boost::asio::async_write(*socket, boost::asio::buffer(write_queue_.front()),
                             [this, socket](const boost::system::error_code& ec, std::size_t) {
                                 if (!running_ || socket != socket_) {
                                     return;
                                 }
                                 if (ec) {
                                     CloseSocket();
                                     return;
                                 }
                                 write_queue_.pop_front();
                                 if (!write_queue_.empty()) {
                                     DoWrite(socket);
                                 }
                             });
}

void BotClient::ScheduleTimer(int delay_ms, std::function<void()> callback) {
    if (!running_) {
        return;
    }
    auto timer = std::make_shared<Timer>(io_context_, std::chrono::milliseconds(std::max(0, delay_ms)));
    timer->async_wait([this, timer, callback = std::move(callback)](const boost::system::error_code& ec) {
        if (!ec && running_) {
            callback();
        }
    });
}

int BotClient::JitterMs(int base_ms) const {
    const int jitter = std::max(0, options_.action_jitter_ms);
    return std::max(0, base_ms + RandomUtil::Int(0, jitter));
}

bool BotClient::PercentHit(int percent) const {
    if (percent <= 0) {
        return false;
    }
    if (percent >= 100) {
        return true;
    }
    return RandomUtil::Int(1, 100) <= percent;
}

void BotClient::Login() {
    proto::LoginRequest request;
    request.set_account("bot_" + std::to_string(index_));
    request.set_token("bot_token");
    SendPacket(ProtoHelper::Build(MSG_LOGIN_REQ, request, ++seq_));
}

void BotClient::Reconnect() {
    proto::ReconnectRequest request;
    request.set_player_id(player_id_);
    request.set_session_token(session_token_);
    SendPacket(ProtoHelper::Build(MSG_RECONNECT_REQ, request, ++seq_));
}

void BotClient::ScheduleHeartbeat() {
    if (heartbeat_started_ || options_.heartbeat_interval_ms <= 0) {
        return;
    }
    heartbeat_started_ = true;
    ScheduleTimer(options_.heartbeat_interval_ms, [this]() { Heartbeat(); });
}

void BotClient::Heartbeat() {
    if (!running_) {
        return;
    }
    if (connected_) {
        proto::HeartbeatRequest request;
        request.set_client_time_ms(NowMs());
        SendPacket(ProtoHelper::Build(MSG_HEARTBEAT_REQ, request, ++seq_));
    }
    ScheduleTimer(options_.heartbeat_interval_ms, [this]() { Heartbeat(); });
}

void BotClient::Match() {
    if (!connected_ || player_id_ == 0 || room_id_ != 0) {
        return;
    }
    proto::MatchRequest request;
    request.set_player_id(player_id_);
    state_ = FlowState::Matching;
    SendPacket(ProtoHelper::Build(MSG_MATCH_REQ, request, ++seq_));
}

void BotClient::CancelMatch() {
    if (!connected_ || state_ != FlowState::Matching || room_id_ != 0) {
        return;
    }
    proto::MatchCancelRequest request;
    request.set_player_id(player_id_);
    SendPacket(ProtoHelper::Build(MSG_MATCH_CANCEL_REQ, request, ++seq_));
    cancelled_this_round_ = true;
    state_ = FlowState::LoggedIn;
    if (stats_) {
        ++stats_->match_cancel;
    }
    ScheduleTimer(JitterMs(options_.rematch_delay_ms), [this]() {
        if (state_ == FlowState::LoggedIn && room_id_ == 0) {
            Match();
        }
    });
}

void BotClient::ScheduleCancelMatch() {
    ScheduleTimer(RandomUtil::Int(50, JitterMs(250)), [this]() { CancelMatch(); });
}

bool BotClient::MaybeScheduleDisconnect(DisconnectPoint point) {
    if (reconnect_count_ >= options_.max_reconnects) {
        return false;
    }

    int percent = 0;
    bool* round_flag = nullptr;
    switch (point) {
    case DisconnectPoint::Queue:
        percent = options_.queue_disconnect_percent;
        round_flag = &queue_disconnect_this_round_;
        break;
    case DisconnectPoint::Room:
        percent = options_.room_disconnect_percent;
        round_flag = &room_disconnect_this_round_;
        break;
    case DisconnectPoint::Playing:
        percent = options_.playing_disconnect_percent;
        round_flag = &playing_disconnect_this_round_;
        break;
    }

    if (!round_flag || *round_flag || !PercentHit(percent)) {
        return false;
    }
    *round_flag = true;
    ScheduleTimer(RandomUtil::Int(50, JitterMs(500)), [this, point]() { SimulateDisconnect(point); });
    return true;
}

void BotClient::SimulateDisconnect(DisconnectPoint point) {
    if (!running_ || !connected_ || reconnect_count_ >= options_.max_reconnects) {
        return;
    }
    ++reconnect_count_;
    if (stats_) {
        ++stats_->disconnects;
    }
    if (options_.verbose) {
        const char* point_name = "unknown";
        switch (point) {
        case DisconnectPoint::Queue:
            point_name = "queue";
            break;
        case DisconnectPoint::Room:
            point_name = "room";
            break;
        case DisconnectPoint::Playing:
            point_name = "playing";
            break;
        }
        std::cout << "bot " << index_ << " disconnect at " << point_name << "\n";
    }
    CloseSocket();
    ScheduleReconnect();
}

void BotClient::Ready() {
    if (!connected_ || room_id_ == 0) {
        return;
    }
    proto::ReadyRequest request;
    request.set_ready(true);
    SendPacket(ProtoHelper::Build(MSG_READY_REQ, request, ++seq_));
    if (stats_) {
        ++stats_->ready;
    }
}

void BotClient::Unready() {
    if (!connected_ || room_id_ == 0) {
        return;
    }
    proto::ReadyRequest request;
    request.set_ready(false);
    SendPacket(ProtoHelper::Build(MSG_READY_REQ, request, ++seq_));
    if (stats_) {
        ++stats_->unready;
    }
}

void BotClient::ScheduleReadyFlow() {
    if (room_id_ == 0) {
        return;
    }

    if (PercentHit(options_.ready_toggle_percent)) {
        Ready();
        ScheduleTimer(RandomUtil::Int(100, JitterMs(600)), [this]() {
            Unready();
            ScheduleTimer(RandomUtil::Int(100, JitterMs(600)), [this]() { Ready(); });
        });
        return;
    }
    Ready();
}

void BotClient::StartInputLoop() {
    if (!running_ || room_id_ == 0 || input_loop_active_) {
        return;
    }
    input_loop_active_ = true;
    SendInput();
}

void BotClient::SendInput() {
    if (!running_ || !connected_ || room_id_ == 0 || !input_loop_active_) {
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
    if (stats_) {
        ++stats_->input_sent;
    }

    ScheduleTimer(options_.input_interval_ms, [this]() { SendInput(); });
}

void BotClient::OnMessage(const Packet& packet) {
    if (packet.msg_id == MSG_LOGIN_RESP) {
        proto::LoginResponse response;
        if (ProtoHelper::Parse(packet, &response) && response.code() == 0) {
            player_id_ = response.player_id();
            session_token_ = response.session_token();
            state_ = FlowState::LoggedIn;
            if (stats_) {
                ++stats_->login_ok;
            }
            ScheduleHeartbeat();
            ScheduleTimer(JitterMs(10), [this]() { Match(); });
        } else if (stats_) {
            ++stats_->login_failed;
        }
        return;
    }

    if (packet.msg_id == MSG_RECONNECT_RESP) {
        proto::ReconnectResponse response;
        if (ProtoHelper::Parse(packet, &response) && response.code() == 0) {
            room_id_ = response.room_id();
            state_ = room_id_ == 0 ? FlowState::LoggedIn : FlowState::InRoom;
            if (stats_) {
                ++stats_->reconnect_ok;
            }
            if (room_id_ != 0) {
                ScheduleReadyFlow();
                StartInputLoop();
            } else {
                ScheduleTimer(JitterMs(options_.rematch_delay_ms), [this]() { Match(); });
            }
        } else if (stats_) {
            ++stats_->reconnect_failed;
        }
        return;
    }

    if (packet.msg_id == MSG_MATCH_RESP) {
        HandleMatchResponse(packet);
        return;
    }

    if (packet.msg_id == MSG_MATCH_SUCCESS_NOTIFY) {
        HandleMatchSuccess(packet);
        return;
    }

    if (packet.msg_id == MSG_ROOM_STATE_NOTIFY) {
        HandleRoomState(packet);
        return;
    }

    if (packet.msg_id == MSG_GAME_STATE_NOTIFY) {
        HandleGameState(packet);
        return;
    }

    if (packet.msg_id == MSG_GAME_OVER_NOTIFY) {
        HandleGameOver();
    }
}

void BotClient::HandleMatchResponse(const Packet& packet) {
    proto::MatchResponse response;
    if (!ProtoHelper::Parse(packet, &response)) {
        return;
    }

    if (response.code() == 0) {
        state_ = FlowState::Matching;
        if (stats_) {
            ++stats_->match_ok;
        }
        if (!cancelled_this_round_ && PercentHit(options_.cancel_match_percent)) {
            ScheduleCancelMatch();
            return;
        }
        MaybeScheduleDisconnect(DisconnectPoint::Queue);
        return;
    }

    if (response.code() == 2) {
        state_ = FlowState::Matching;
        return;
    }

    if (stats_) {
        ++stats_->match_failed;
    }
    state_ = FlowState::LoggedIn;
    ScheduleTimer(JitterMs(options_.rematch_delay_ms), [this]() { Match(); });
}

void BotClient::HandleMatchSuccess(const Packet& packet) {
    proto::MatchSuccessNotify notify;
    if (!ProtoHelper::Parse(packet, &notify)) {
        return;
    }
    room_id_ = notify.room_id();
    state_ = FlowState::InRoom;
    input_loop_active_ = false;
    if (stats_) {
        ++stats_->match_success;
    }

    const bool will_disconnect_before_ready = MaybeScheduleDisconnect(DisconnectPoint::Room);
    if (will_disconnect_before_ready) {
        return;
    }
    ScheduleTimer(JitterMs(50), [this]() {
        if (connected_ && room_id_ != 0) {
            ScheduleReadyFlow();
        }
    });
}

void BotClient::HandleRoomState(const Packet& packet) {
    proto::RoomStateNotify notify;
    if (!ProtoHelper::Parse(packet, &notify)) {
        return;
    }
    room_id_ = notify.room_id();
    if (notify.state() == 2) {
        state_ = FlowState::Playing;
        if (stats_) {
            ++stats_->room_playing;
        }
        StartInputLoop();
        MaybeScheduleDisconnect(DisconnectPoint::Playing);
    }
}

void BotClient::HandleGameState(const Packet&) {
    if (stats_) {
        ++stats_->game_state;
    }
    if (room_id_ != 0 && state_ != FlowState::Playing) {
        state_ = FlowState::Playing;
        StartInputLoop();
        MaybeScheduleDisconnect(DisconnectPoint::Playing);
    }
}

void BotClient::HandleGameOver() {
    room_id_ = 0;
    input_loop_active_ = false;
    state_ = FlowState::LoggedIn;
    cancelled_this_round_ = false;
    queue_disconnect_this_round_ = false;
    room_disconnect_this_round_ = false;
    playing_disconnect_this_round_ = false;
    reconnect_count_ = 0;
    input_seq_ = 0;
    if (stats_) {
        ++stats_->game_over;
    }
    ScheduleTimer(JitterMs(options_.rematch_delay_ms), [this]() { Match(); });
}

} // namespace game
