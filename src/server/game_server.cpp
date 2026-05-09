#include "server/game_server.h"

#include "config/config_manager.h"
#include "game/state_sync.h"
#include "protocol/message_id.h"
#include "util/logger.h"
#include "util/time_util.h"

#include "game.pb.h"

namespace game {

bool GameServer::Init() {
    context_ = std::make_unique<ServiceContext>();
    dispatcher_ = std::make_unique<MessageDispatcher>();
    connection_manager_ = std::make_unique<ConnectionManager>();

    mysql_client_ = std::make_unique<MysqlClient>();
    redis_client_ = std::make_unique<RedisClient>();
    mysql_client_->Connect();
    redis_client_->Connect();

    player_repository_ = std::make_unique<PlayerRepository>(mysql_client_.get());
    battle_repository_ = std::make_unique<BattleRepository>(mysql_client_.get());

    player_manager_ = std::make_unique<PlayerManager>();
    game_loop_ = std::make_unique<GameLoop>(ConfigManager::Instance().TickIntervalMs());
    rank_service_ = std::make_unique<RankService>(redis_client_.get());
    settlement_service_ = std::make_unique<SettlementService>(
        battle_repository_.get(), player_repository_.get(), rank_service_.get());

    context_->connection_manager = connection_manager_.get();
    context_->player_manager = player_manager_.get();
    context_->game_loop = game_loop_.get();
    context_->rank_service = rank_service_.get();
    context_->settlement_service = settlement_service_.get();
    context_->mysql_client = mysql_client_.get();
    context_->redis_client = redis_client_.get();
    context_->player_repository = player_repository_.get();
    context_->battle_repository = battle_repository_.get();

    room_manager_ = std::make_unique<RoomManager>(context_.get());
    login_service_ = std::make_unique<LoginService>(context_.get());
    match_service_ = std::make_unique<MatchService>(context_.get());

    context_->room_manager = room_manager_.get();
    context_->login_service = login_service_.get();
    context_->match_service = match_service_.get();

    game_loop_->SetTickCallback([this](int64_t room_id, const GameState& state, bool game_over, int64_t winner_id) {
        auto room = room_manager_->GetRoom(room_id);
        if (!room) {
            return;
        }

        auto state_notify = StateSync::BuildNotify(room_id, state);
        room->Broadcast(MSG_GAME_STATE_NOTIFY, state_notify);

        if (!game_over) {
            return;
        }

        std::vector<int64_t> losers;
        for (const auto& [player_id, entity] : state.players) {
            if (player_id != winner_id) {
                losers.push_back(player_id);
            }
        }

        if (settlement_service_) {
            settlement_service_->SettleBattle(room_id, winner_id, losers);
        }

        proto::GameOverNotify over;
        over.set_room_id(room_id);
        over.set_winner_id(winner_id);
        for (auto loser_id : losers) {
            over.add_loser_ids(loser_id);
        }
        room->Broadcast(MSG_GAME_OVER_NOTIFY, over);
        room->SetSettlement();
        room->Close();
        room_manager_->RemoveRoom(room_id);
    });

    rank_service_->Init();
    login_service_->RegisterHandlers(*dispatcher_);
    match_service_->RegisterHandlers(*dispatcher_);
    room_manager_->RegisterHandlers(*dispatcher_);
    rank_service_->RegisterHandlers(*dispatcher_);

    timer_manager_ = std::make_unique<TimerManager>();
    timer_manager_->AddTimer(5000, [this]() {
        const auto now = NowMs();
        for (auto& session : connection_manager_->AllSessions()) {
            if (session && now - session->LastHeartbeatMs() > 30000) {
                LOG_WARN("heartbeat timeout session {}", session->SessionId());
                session->Close();
            }
        }
    });

    tcp_server_ = std::make_unique<TcpServer>(
        *connection_manager_,
        [this](const SessionPtr& session, const Packet& packet) {
            dispatcher_->Dispatch(session, packet);
        },
        ConfigManager::Instance().WorkerThreads());

    return true;
}

void GameServer::Start() {
    game_loop_->Start();
    timer_manager_->Start();
    const auto& config = ConfigManager::Instance();
    tcp_server_->Start(config.ServerHost(), static_cast<uint16_t>(config.ServerPort()));
}

void GameServer::Stop() {
    if (tcp_server_) {
        tcp_server_->Stop();
    }
    if (timer_manager_) {
        timer_manager_->Stop();
    }
    if (game_loop_) {
        game_loop_->Stop();
    }
}

} // namespace game
