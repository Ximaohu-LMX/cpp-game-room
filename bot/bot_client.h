#pragma once

#include "net/codec.h"
#include "net/packet.h"

#include <boost/asio.hpp>

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <string>
#include <thread>

namespace game {

struct BotOptions {
    int cancel_match_percent = 10;
    int queue_disconnect_percent = 5;
    int room_disconnect_percent = 5;
    int playing_disconnect_percent = 5;
    int ready_toggle_percent = 10;
    int max_reconnects = 2;
    int reconnect_delay_ms = 500;
    int rematch_delay_ms = 500;
    int action_jitter_ms = 800;
    int heartbeat_interval_ms = 5000;
    int input_interval_ms = 50;
    bool verbose = false;
};

struct BotStats {
    std::atomic<int64_t> connect_ok{0};
    std::atomic<int64_t> connect_failed{0};
    std::atomic<int64_t> disconnects{0};
    std::atomic<int64_t> login_ok{0};
    std::atomic<int64_t> login_failed{0};
    std::atomic<int64_t> reconnect_ok{0};
    std::atomic<int64_t> reconnect_failed{0};
    std::atomic<int64_t> match_ok{0};
    std::atomic<int64_t> match_failed{0};
    std::atomic<int64_t> match_cancel{0};
    std::atomic<int64_t> match_success{0};
    std::atomic<int64_t> ready{0};
    std::atomic<int64_t> unready{0};
    std::atomic<int64_t> room_playing{0};
    std::atomic<int64_t> input_sent{0};
    std::atomic<int64_t> game_state{0};
    std::atomic<int64_t> game_over{0};
};

class BotClient {
public:
    BotClient(std::string host, uint16_t port, int index, BotOptions options, std::shared_ptr<BotStats> stats);
    ~BotClient();

    void Start();
    void Stop();

private:
    using TcpSocket = boost::asio::ip::tcp::socket;
    using Timer = boost::asio::steady_timer;

    enum class FlowState {
        Disconnected,
        Connecting,
        LoggedIn,
        Matching,
        InRoom,
        Playing
    };

    enum class DisconnectPoint {
        Queue,
        Room,
        Playing
    };

    void Connect(bool reconnect);
    void ScheduleReconnect();
    void CloseSocket();
    void DoRead(const std::shared_ptr<TcpSocket>& socket);
    void SendPacket(const Packet& packet);
    void DoWrite(const std::shared_ptr<TcpSocket>& socket);
    void ScheduleTimer(int delay_ms, std::function<void()> callback);
    int JitterMs(int base_ms) const;
    bool PercentHit(int percent) const;

    void Login();
    void Reconnect();
    void ScheduleHeartbeat();
    void Heartbeat();
    void Match();
    void CancelMatch();
    void ScheduleCancelMatch();
    bool MaybeScheduleDisconnect(DisconnectPoint point);
    void SimulateDisconnect(DisconnectPoint point);
    void Ready();
    void Unready();
    void ScheduleReadyFlow();
    void StartInputLoop();
    void SendInput();
    void OnMessage(const Packet& packet);
    void HandleMatchResponse(const Packet& packet);
    void HandleMatchSuccess(const Packet& packet);
    void HandleRoomState(const Packet& packet);
    void HandleGameState(const Packet& packet);
    void HandleGameOver();

    std::string host_;
    uint16_t port_;
    int index_;
    BotOptions options_;
    std::shared_ptr<BotStats> stats_;

    boost::asio::io_context io_context_;
    std::unique_ptr<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>> work_guard_;
    std::shared_ptr<TcpSocket> socket_;
    std::thread worker_;
    std::atomic<bool> running_{false};

    Codec codec_;
    Buffer read_buffer_;
    std::array<char, 4096> read_temp_{};
    std::deque<std::string> write_queue_;
    FlowState state_ = FlowState::Disconnected;
    bool connected_ = false;
    bool heartbeat_started_ = false;
    bool input_loop_active_ = false;
    bool cancelled_this_round_ = false;
    bool queue_disconnect_this_round_ = false;
    bool room_disconnect_this_round_ = false;
    bool playing_disconnect_this_round_ = false;
    int reconnect_count_ = 0;
    uint32_t seq_ = 0;
    int64_t player_id_ = 0;
    int64_t room_id_ = 0;
    int64_t input_seq_ = 0;
    std::string session_token_;
};

} // namespace game
