#include "bot_client.h"

#include "protocol/message_id.h"
#include "protocol/proto_helper.h"
#include "util/time_util.h"

#include "game.pb.h"
#include "login.pb.h"
#include "match.pb.h"
#include "room.pb.h"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <utility>
#include <sstream>

namespace game {

BotClient::BotClient(boost::asio::io_context& io,
                     boost::asio::ip::tcp::resolver::results_type endpoints,
                     int index, BotOptions options, std::shared_ptr<BotStats> stats)
    : endpoints_(std::move(endpoints)), index_(index), options_(options),
      stats_(std::move(stats)), strand_(boost::asio::make_strand(io)) {
    std::seed_seq seed{static_cast<uint32_t>(options.seed),
                       static_cast<uint32_t>(options.seed >> 32), static_cast<uint32_t>(index)};
    random_.seed(seed);
}

void BotClient::Start() {
    boost::asio::post(strand_, [self = shared_from_this()] {
        if (self->running_) return;
        self->running_ = true;
        self->Connect(false);
    });
}

void BotClient::Stop() {
    boost::asio::post(strand_, [self = shared_from_this()] {
        self->running_ = false;
        self->CloseSocket();
    });
}

void BotClient::Connect(bool reconnect) {
    if (!running_) return;
    CloseSocket();
    state_ = FlowState::Connecting;
    socket_ = std::make_shared<TcpSocket>(strand_);
    read_ = std::make_shared<ReadState>();
    auto socket = socket_;
    boost::asio::async_connect(*socket, endpoints_, boost::asio::bind_executor(strand_,
        [this, self = shared_from_this(), socket, reconnect](const boost::system::error_code& ec, const auto&) {
            if (!running_ || socket != socket_) return;
            if (ec) {
                ++stats_->connect_failed;
                ScheduleTimer(options_.reconnect_delay_ms, [this, reconnect] { Connect(reconnect); },
                              TimerScope::Lifetime);
                return;
            }
            connected_ = true;
            ++stats_->connect_ok;
            ++stats_->connections_live;
            if (reconnect && player_id_ != 0 && !session_token_.empty()) Reconnect();
            else Login();
            DoRead(socket, read_);
        }));
}

void BotClient::ScheduleReconnect() {
    if (!running_ || player_id_ == 0 || session_token_.empty()) {
        return;
    }
    ScheduleTimer(JitterMs(options_.reconnect_delay_ms), [this]() { Connect(true); }, TimerScope::Lifetime);
}

void BotClient::ExpireHeartbeats() {
    const auto now = Metrics::SteadyUs();
    for (auto it = heartbeat_started_us_.begin(); it != heartbeat_started_us_.end();) {
        if (now - it->second >= 30000000) {
            if (stats_) { ++stats_->heartbeat_timeouts; --stats_->heartbeat_pending; }
            it = heartbeat_started_us_.erase(it);
        } else {
            ++it;
        }
    }
}

void BotClient::MarkDisconnected(bool unexpected) {
    if (connected_ && stats_) {
        --stats_->connections_live;
        if (unexpected) ++stats_->unexpected_disconnects;
    }
    connected_ = false;
    if (stats_) {
        stats_->heartbeat_cancelled += heartbeat_started_us_.size();
        stats_->heartbeat_pending -= heartbeat_started_us_.size();
    }
    heartbeat_started_us_.clear();
}

void BotClient::CloseSocket() {
    ++connection_generation_;
    ++input_generation_;
    MarkDisconnected(false);
    FinishRecovery("cancelled");
    input_loop_active_ = false;
    heartbeat_started_ = false;
    last_input_us_ = 0;
    for (const auto& timer : timers_) timer->cancel();
    timers_.clear();
    for (const auto& write : write_queue_) if (write->msg_id == MSG_INPUT_REQ) {
        ++stats_->input_cancelled;
        --stats_->input_pending;
    }
    write_queue_.clear(); // In-flight write captures its buffer separately.
    auto socket = std::move(socket_);
    read_.reset(); // Old read callbacks retain their own buffers.
    if (socket) {
        boost::system::error_code ignored;
        socket->shutdown(TcpSocket::shutdown_both, ignored);
        socket->close(ignored);
    }
}

void BotClient::DoRead(const std::shared_ptr<TcpSocket>& socket, const std::shared_ptr<ReadState>& read) {
    socket->async_read_some(boost::asio::buffer(read->temporary), boost::asio::bind_executor(strand_,
        [this, self = shared_from_this(), socket, read](const boost::system::error_code& ec, std::size_t bytes) {
            if (!running_ || socket != socket_) return;
            if (ec) { MarkDisconnected(true); CloseSocket(); return; }
            read->buffer.insert(read->buffer.end(), read->temporary.data(), read->temporary.data() + bytes);
            try {
                const auto packets = codec_.Decode(read->buffer);
                for (const auto& packet : packets) {
                    if (!running_ || socket != socket_) break;
                    OnMessage(packet);
                }
            } catch (const std::exception&) {
                MarkDisconnected(true); CloseSocket(); return;
            }
            if (socket == socket_) DoRead(socket, read);
        }));
}

void BotClient::SendPacket(const Packet& packet) {
    const auto posted = Metrics::SteadyUs();
    const auto generation = connection_generation_, input_generation = input_generation_;
    const bool input = packet.msg_id == MSG_INPUT_REQ;
    if (input) ++stats_->input_pending;
    boost::asio::post(strand_, [this, self = shared_from_this(), packet, posted, generation, input_generation, input] {
        if (!running_ || !connected_ || !socket_ || generation != connection_generation_ ||
            (input && input_generation != input_generation_)) {
            if (input) { ++stats_->input_dropped; --stats_->input_pending; }
            return;
        }
        if (input) stats_->Observe(stats_->input_post_late, Metrics::SteadyUs() - posted);
        const bool writing = !write_queue_.empty();
        write_queue_.push_back(std::make_shared<PendingWrite>(PendingWrite{codec_.Encode(packet), packet.msg_id, posted}));
        if (!writing) DoWrite(socket_);
    });
}

void BotClient::DoWrite(const std::shared_ptr<TcpSocket>& socket) {
    if (socket != socket_ || write_queue_.empty()) return;
    auto write = write_queue_.front();
    if (write->msg_id == MSG_INPUT_REQ)
        stats_->Observe(stats_->input_write_wait, Metrics::SteadyUs() - write->enqueued_us);
    boost::asio::async_write(*socket, boost::asio::buffer(write->bytes), boost::asio::bind_executor(strand_,
        [this, self = shared_from_this(), socket, write](const boost::system::error_code& ec, std::size_t) {
            if (!running_ || socket != socket_) return;
            if (ec) { MarkDisconnected(true); CloseSocket(); return; }
            ++stats_->packets_written;
            if (write->msg_id == MSG_INPUT_REQ) { ++stats_->input_written; --stats_->input_pending; }
            write_queue_.pop_front();
            if (!write_queue_.empty()) DoWrite(socket);
        }));
}

void BotClient::ScheduleTimer(int delay_ms, std::function<void()> callback, TimerScope scope, bool input) {
    if (!running_) return;
    auto timer = std::make_shared<Timer>(strand_);
    timer->expires_after(std::chrono::milliseconds(std::max(0, delay_ms)));
    const auto due = std::chrono::duration_cast<std::chrono::microseconds>(timer->expiry().time_since_epoch()).count();
    const auto connection = connection_generation_, round = round_generation_, input_generation = input_generation_;
    timers_.insert(timer);
    timer->async_wait(boost::asio::bind_executor(strand_,
        [this, self = shared_from_this(), timer, callback = std::move(callback), scope, input,
         due, connection, round, input_generation](const boost::system::error_code& ec) {
            timers_.erase(timer);
            if (ec || !running_) return;
            if (scope != TimerScope::Lifetime && connection != connection_generation_) return;
            if (scope == TimerScope::Round && round != round_generation_) return;
            if (input) {
                if (input_generation != input_generation_ || !input_loop_active_) return;
                const auto now = Metrics::SteadyUs();
                stats_->Observe(stats_->input_timer_late, now > static_cast<uint64_t>(due) ? now - due : 0);
            }
            callback();
        }));
}

int BotClient::RandomInt(int low, int high) { return std::uniform_int_distribution<int>(low, high)(random_); }
float BotClient::RandomFloat(float low, float high) { return std::uniform_real_distribution<float>(low, high)(random_); }
int BotClient::JitterMs(int base_ms) { return std::max(0, base_ms + RandomInt(0, std::max(0, options_.action_jitter_ms))); }
bool BotClient::PercentHit(int percent) {
    return percent >= 100 || (percent > 0 && RandomInt(1, 100) <= percent);
}

void BotClient::Login() {
    proto::LoginRequest request;
    request.set_account(options_.account_prefix + std::to_string(index_));
    if (stats_) ++stats_->login_attempts;
    login_started_us_ = stats_->measurements.load() ? Metrics::SteadyUs() : 0;
    request.set_token("bot_token");
    SendPacket(ProtoHelper::Build(MSG_LOGIN_REQ, request, ++seq_));
}

void BotClient::Reconnect() {
    ++stats_->reconnect_attempts;
    ++stats_->recovery_pending;
    recovery_started_us_ = Metrics::SteadyUs();
    recovery_room_ = room_id_;
    recovery_frame_ = last_state_.room == room_id_ ? last_state_.frame : -1;
    recovery_hp_ = last_state_.room == room_id_ ? last_state_.hp : -1;
    recovery_response_room_ = -1;
    recovery_sample_ = {};
    recovery_ack_ = false;
    const auto attempt = ++recovery_id_;
    ScheduleTimer(options_.recovery_timeout_ms, [this, attempt] {
        if (attempt == recovery_id_) FinishRecovery("timeout");
    }, TimerScope::Connection);
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
    ScheduleTimer(options_.heartbeat_interval_ms, [this]() { Heartbeat(); }, TimerScope::Connection);
}

void BotClient::Heartbeat() {
    if (!running_) {
        return;
    }
    ExpireHeartbeats();
    if (connected_) {
        proto::HeartbeatRequest request;
        request.set_client_time_ms(NowMs());
        const auto seq = ++seq_;
        if (stats_->measurements.load()) {
            heartbeat_started_us_[seq] = Metrics::SteadyUs();
            if (stats_) { ++stats_->heartbeat_sent; ++stats_->heartbeat_pending; }
        }
        SendPacket(ProtoHelper::Build(MSG_HEARTBEAT_REQ, request, seq));
    }
    ScheduleTimer(options_.heartbeat_interval_ms, [this]() { Heartbeat(); }, TimerScope::Connection);
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
    ScheduleTimer(RandomInt(50, JitterMs(250)), [this]() { CancelMatch(); });
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
    ScheduleTimer(RandomInt(50, JitterMs(500)), [this, point]() { SimulateDisconnect(point); });
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
        ScheduleTimer(RandomInt(100, JitterMs(600)), [this]() {
            Unready();
            ScheduleTimer(RandomInt(100, JitterMs(600)), [this]() { Ready(); });
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
    ++input_generation_;
    last_input_us_ = 0;
    SendInput();
}

void BotClient::SendInput() {
    if (!running_ || !connected_ || room_id_ == 0 || !input_loop_active_) {
        return;
    }
    const auto now = Metrics::SteadyUs();
    if (last_input_us_) stats_->Observe(stats_->input_interval, now - last_input_us_);
    last_input_us_ = now;
    proto::InputRequest request;
    auto* input = request.mutable_input();
    input->set_player_id(player_id_);
    input->set_input_seq(++input_seq_);
    input->set_move_x(RandomFloat(-1.0f, 1.0f));
    input->set_move_y(RandomFloat(-1.0f, 1.0f));
    input->set_fire(RandomInt(0, 3) == 0);
    SendPacket(ProtoHelper::Build(MSG_INPUT_REQ, request, ++seq_));
    if (stats_) {
        ++stats_->input_sent;
    }

    ScheduleTimer(options_.input_interval_ms, [this]() { SendInput(); }, TimerScope::Round, true);
}

void BotClient::OnMessage(const Packet& packet) {
    if (packet.msg_id == MSG_HEARTBEAT_RESP) {
        ExpireHeartbeats();
        auto it = heartbeat_started_us_.find(packet.seq);
        if (it != heartbeat_started_us_.end()) {
            stats_->Observe(stats_->heartbeat_rtt, Metrics::SteadyUs() - it->second);
            heartbeat_started_us_.erase(it);
            if (stats_) { ++stats_->heartbeat_responses; --stats_->heartbeat_pending; }
        }
        return;
    }
    if (packet.msg_id == MSG_LOGIN_RESP) {
        proto::LoginResponse response;
        if (ProtoHelper::Parse(packet, &response) && response.code() == 0) {
            if (login_started_us_)
                stats_->Observe(stats_->login_rtt, Metrics::SteadyUs() - login_started_us_);
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
            if (recovery_started_us_)
                stats_->Observe(stats_->reconnect_rtt, Metrics::SteadyUs() - recovery_started_us_);
            recovery_ack_ = true;
            room_id_ = response.room_id();
            recovery_response_room_ = room_id_;
            ScheduleHeartbeat();
            if (recovery_started_us_) {
                if (room_id_ == 0) FinishRecovery("no_room");
                else if (room_id_ != recovery_room_) FinishRecovery("invalid");
                else TryFinishRecovery();
            }
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
        } else {
            ++stats_->reconnect_failed;
            FinishRecovery("rejected");
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
    ++round_generation_;
    ++input_generation_;
    last_state_ = {};
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

void BotClient::HandleGameState(const Packet& packet) {
    proto::GameStateNotify notify;
    if (!ProtoHelper::Parse(packet, &notify)) return;
    ++stats_->game_state;
    for (const auto& player : notify.players()) if (player.player_id() == player_id_) {
        last_state_ = {notify.room_id(), notify.frame_id(), player.hp()};
        if (recovery_started_us_) { recovery_sample_ = last_state_; TryFinishRecovery(); }
        break;
    }
    if (room_id_ != 0 && state_ != FlowState::Playing) {
        state_ = FlowState::Playing;
        StartInputLoop();
        MaybeScheduleDisconnect(DisconnectPoint::Playing);
    }
}

void BotClient::TryFinishRecovery() {
    if (!recovery_started_us_ || !recovery_ack_ || recovery_sample_.frame < 0) return;
    if (recovery_sample_.room != recovery_room_ || recovery_sample_.frame < recovery_frame_)
        FinishRecovery("invalid", recovery_sample_.frame, recovery_sample_.hp);
    else FinishRecovery("state", recovery_sample_.frame, recovery_sample_.hp);
}

void BotClient::FinishRecovery(const char* outcome, int64_t frame, int hp) {
    if (!recovery_started_us_) return;
    const auto elapsed = Metrics::SteadyUs() - recovery_started_us_;
    const std::string result(outcome);
    if (result == "state") { ++stats_->recovery_state; stats_->Observe(stats_->recovery_latency, elapsed); }
    else if (result == "no_room") ++stats_->recovery_no_room;
    else if (result == "rejected") ++stats_->recovery_rejected;
    else if (result == "timeout") ++stats_->recovery_timeout;
    else if (result == "terminal") ++stats_->recovery_terminal;
    else if (result == "invalid") ++stats_->recovery_invalid;
    else ++stats_->recovery_cancelled;
    --stats_->recovery_pending;
    std::ostringstream event;
    event << "{\"bot\":" << index_ << ",\"attempt\":" << recovery_id_
          << ",\"player_id\":" << player_id_ << ",\"expected_room\":" << recovery_room_
          << ",\"response_room\":" << recovery_response_room_ << ",\"before_frame\":" << recovery_frame_
          << ",\"before_hp\":" << recovery_hp_ << ",\"observed_frame\":" << frame << ",\"observed_hp\":" << hp
          << ",\"elapsed_us\":" << elapsed << ",\"outcome\":\"" << result << "\"}";
    stats_->RecoveryEvent(event.str());
    recovery_started_us_ = 0;
}

void BotClient::HandleGameOver() {
    FinishRecovery("terminal");
    ++round_generation_;
    ++input_generation_;
    last_input_us_ = 0;
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
