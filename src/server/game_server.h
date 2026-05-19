#pragma once

#include "game/game_loop.h"
#include "game/settlement_service.h"
#include "login/login_service.h"
#include "match/match_service.h"
#include "net/connection_manager.h"
#include "net/tcp_server.h"
#include "player/player_manager.h"
#include "rank/rank_service.h"
#include "room/room_manager.h"
#include "server/message_dispatcher.h"
#include "server/service_context.h"
#include "storage/battle_repository.h"
#include "storage/mysql_client.h"
#include "storage/player_repository.h"
#include "storage/redis_client.h"
#include "timer/timer_manager.h"

#include <memory>

namespace game {

/**
 * @brief 游戏服务器总控类。
 * @help 负责初始化所有模块、完成依赖注入、注册消息处理器并启动网络/定时器/GameLoop。
 */
class GameServer {
public:
    /**
     * @brief 初始化服务器模块。
     * @return 初始化成功返回 true，依赖连接失败返回 false。
     */
    bool Init();

    /**
     * @brief 启动服务器。
     * @help 启动 GameLoop、TimerManager 和 TCP 监听。
     */
    void Start();

    /**
     * @brief 停止服务器。
     */
    void Stop();

private:
    std::unique_ptr<ServiceContext> context_;
    std::unique_ptr<MessageDispatcher> dispatcher_;
    std::unique_ptr<ConnectionManager> connection_manager_;

    std::unique_ptr<MysqlClient> mysql_client_;
    std::unique_ptr<RedisClient> redis_client_;
    std::unique_ptr<PlayerRepository> player_repository_;
    std::unique_ptr<BattleRepository> battle_repository_;

    std::unique_ptr<PlayerManager> player_manager_;
    std::unique_ptr<GameLoop> game_loop_;
    std::unique_ptr<RankService> rank_service_;
    std::unique_ptr<SettlementService> settlement_service_;
    std::unique_ptr<RoomManager> room_manager_;
    std::unique_ptr<LoginService> login_service_;
    std::unique_ptr<MatchService> match_service_;
    std::unique_ptr<TimerManager> timer_manager_;
    std::unique_ptr<TcpServer> tcp_server_;
};

} // namespace game
