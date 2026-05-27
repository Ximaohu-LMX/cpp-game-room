#include "server/game_server.h"

#include "config/config_manager.h"
#include "game/state_sync.h"
#include "protocol/message_id.h"
#include "util/logger.h"
#include "util/time_util.h"

#include "game.pb.h"

namespace game {

bool GameServer::Init() {
    // 创建服务端基础组件：上下文用于依赖注入，dispatcher 负责按 msg_id 分发消息。
    context_ = std::make_unique<ServiceContext>();
    dispatcher_ = std::make_unique<MessageDispatcher>();
    connection_manager_ = std::make_unique<ConnectionManager>();

    // 初始化真实存储客户端：MySQL 负责持久化，Redis 负责排行榜。
    mysql_client_ = std::make_unique<MysqlClient>();
    redis_client_ = std::make_unique<RedisClient>();
    if (!mysql_client_->Connect()) {
        LOG_ERROR("failed to connect mysql");
        return false;
    }
    if (!redis_client_->Connect()) {
        LOG_ERROR("failed to connect redis");
        return false;
    }

    // Repository 封装数据库访问，业务层通过仓储读写玩家和战斗数据。
    player_repository_ = std::make_unique<PlayerRepository>(mysql_client_.get());
    battle_repository_ = std::make_unique<BattleRepository>(mysql_client_.get());

    // 创建核心业务模块：玩家管理、固定 tick 游戏循环、排行榜和战斗结算。
    player_manager_ = std::make_unique<PlayerManager>();
    game_loop_ = std::make_unique<GameLoop>(ConfigManager::Instance().TickIntervalMs());
    rank_service_ = std::make_unique<RankService>(redis_client_.get());
    settlement_service_ = std::make_unique<SettlementService>(
        battle_repository_.get(), player_repository_.get(), rank_service_.get());

    // 先把底层依赖写入 ServiceContext，后续业务服务通过 context 访问共享模块。
    context_->connection_manager = connection_manager_.get();
    context_->player_manager = player_manager_.get();
    context_->game_loop = game_loop_.get();
    context_->rank_service = rank_service_.get();
    context_->settlement_service = settlement_service_.get();
    context_->mysql_client = mysql_client_.get();
    context_->redis_client = redis_client_.get();
    context_->player_repository = player_repository_.get();
    context_->battle_repository = battle_repository_.get();

    // 创建依赖 context 的业务服务。
    room_manager_ = std::make_unique<RoomManager>(context_.get());
    login_service_ = std::make_unique<LoginService>(context_.get());
    match_service_ = std::make_unique<MatchService>(context_.get());

    // 再把业务服务写回 context，形成统一的服务访问入口。
    context_->room_manager = room_manager_.get();
    context_->login_service = login_service_.get();
    context_->match_service = match_service_.get();


    // GameLoop 每个 tick 后回调这里：广播状态；如果游戏结束，则结算、通知 GameOver 并关闭房间。
    // 只做广播和结算，不动state数据，state数据由 GameRoom 内的游戏逻辑更新并传入回调。
    game_loop_->SetTickCallback([this](int64_t room_id, const GameState& state, bool game_over, int64_t winner_id) {
        auto room = room_manager_->GetRoom(room_id);
        if (!room) {
            return;
        }

        // 每帧把当前 GameState 转成 protobuf 通知，广播给房间内玩家。（由客户端处理广播收到的消息，更新玩家界面）
        auto state_notify = StateSync::BuildNotify(room_id, state);
        room->Broadcast(MSG_GAME_STATE_NOTIFY, state_notify);

        if (!game_over) {
            return;
        }

        // 吃鸡式单胜者：winner_id 之外的存活/死亡玩家都作为失败方进入结算。
        std::vector<int64_t> losers;
        for (const auto& [player_id, entity] : state.players) {
            if (player_id != winner_id) {
                losers.push_back(player_id);
            }
        }

        if (settlement_service_) {
            settlement_service_->SettleBattle(room_id, winner_id, losers);
        }

        // 广播本局结束通知，然后把房间推进到结算/关闭状态并从管理器移除。
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

    // 注册所有业务消息处理函数：网络层收到 Packet 后只需要交给 dispatcher。
    rank_service_->Init();
    login_service_->RegisterHandlers(*dispatcher_);
    match_service_->RegisterHandlers(*dispatcher_);
    room_manager_->RegisterHandlers(*dispatcher_);
    rank_service_->RegisterHandlers(*dispatcher_);

    // 心跳超时检查：每 5 秒扫描一次连接，30 秒无心跳则关闭 Session。
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

    // TCP 网络入口：收到完整 Packet 后，通过 dispatcher 分发给对应业务 service。
    tcp_server_ = std::make_unique<TcpServer>(
        *connection_manager_,
        [this](const SessionPtr& session, const Packet& packet) {
            dispatcher_->Dispatch(session, packet);
        },
        ConfigManager::Instance().WorkerThreads(),
        [this](const SessionPtr& session) {
            if (!session || session->PlayerId() == 0) {
                return;
            }

            if (auto current = connection_manager_->GetByPlayerId(session->PlayerId())) {
                if (current->SessionId() != session->SessionId()) {
                    return;
                }
            }

            if (player_manager_) {
                player_manager_->SetOffline(session->PlayerId());
            }
            if (room_manager_ && session->RoomId() != 0) {
                if (auto room = room_manager_->GetRoom(session->RoomId())) {
                    room->OnPlayerDisconnect(session->PlayerId());
                }
            }
        });

    return true;
}

void GameServer::Start() {
    // 启动顺序：先启动游戏循环和定时器，再启动 TCP 监听。
    game_loop_->Start();
    timer_manager_->Start();
    const auto& config = ConfigManager::Instance();
    tcp_server_->Start(config.ServerHost(), static_cast<uint16_t>(config.ServerPort()));
}

void GameServer::Stop() {
    // 停止顺序：先停网络入口，再停定时器和游戏循环，避免继续接收新请求。
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
