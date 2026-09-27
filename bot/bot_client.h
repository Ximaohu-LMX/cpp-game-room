#pragma once

#include "net/codec.h"
#include "net/packet.h"
#include "bot_metrics.h"
#include <unordered_map>

#include <boost/asio.hpp>

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <string>
#include <random>
#include <unordered_set>

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
    int io_threads = 4;
    uint64_t seed = 1;
    int recovery_timeout_ms = 30000;
    bool verbose = false;
    std::string account_prefix = "bot_";
};


class BotClient : public std::enable_shared_from_this<BotClient> {
public:
    BotClient(boost::asio::io_context& io,
              boost::asio::ip::tcp::resolver::results_type endpoints,
              int index, BotOptions options, std::shared_ptr<BotStats> stats);

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
    void MarkDisconnected(bool unexpected);
    void ExpireHeartbeats();
    struct ReadState {
        Buffer buffer;
        std::array<char, 4096> temporary{};
    };
    void DoRead(const std::shared_ptr<TcpSocket>& socket, const std::shared_ptr<ReadState>& read);
    void SendPacket(const Packet& packet);
    void DoWrite(const std::shared_ptr<TcpSocket>& socket);
    enum class TimerScope { Lifetime, Connection, Round };
    void ScheduleTimer(int delay_ms, std::function<void()> callback,
                       TimerScope scope = TimerScope::Round, bool input = false);
    int JitterMs(int base_ms);
    bool PercentHit(int percent);
    int RandomInt(int low, int high);
    float RandomFloat(float low, float high);
    void FinishRecovery(const char* outcome, int64_t frame = -1, int hp = -1);
    void TryFinishRecovery();

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

    boost::asio::ip::tcp::resolver::results_type endpoints_;
    int index_;
    BotOptions options_;
    std::shared_ptr<BotStats> stats_;

    boost::asio::strand<boost::asio::io_context::executor_type> strand_;
    std::shared_ptr<TcpSocket> socket_;
    std::shared_ptr<ReadState> read_;
    std::unordered_set<std::shared_ptr<Timer>> timers_;
    bool running_ = false; // Only accessed on strand_; manager drains before destruction.
    uint64_t connection_generation_ = 0, round_generation_ = 0, input_generation_ = 0;
    std::mt19937_64 random_;
    Codec codec_;
    struct PendingWrite {
        std::string bytes;
        uint32_t msg_id;
        uint64_t enqueued_us;
    };
    std::deque<std::shared_ptr<PendingWrite>> write_queue_;
    uint64_t login_started_us_ = 0;
    uint64_t last_input_us_ = 0;
    uint64_t recovery_started_us_ = 0, recovery_id_ = 0;
    int64_t recovery_room_ = 0, recovery_frame_ = -1, recovery_response_room_ = -1;
    int recovery_hp_ = -1;
    bool recovery_ack_ = false;
    struct StateSample { int64_t room = 0, frame = -1; int hp = -1; };
    StateSample last_state_, recovery_sample_;
    std::unordered_map<uint32_t, uint64_t> heartbeat_started_us_;
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
