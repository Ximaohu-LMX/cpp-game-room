#pragma once

namespace game {

class BattleRepository;
class ConnectionManager;
class GameLoop;
class LoginService;
class MatchService;
class MysqlClient;
class PlayerManager;
class PlayerRepository;
class RankService;
class RedisClient;
class RoomManager;
class SettlementService;

/**
 * @brief 服务依赖上下文。
 * @help 用于把 GameServer 创建的模块注入到 LoginService、MatchService、RoomManager 等业务服务中。
 */
class ServiceContext {
public:
    ConnectionManager* connection_manager = nullptr; ///< 连接管理器。
    PlayerManager* player_manager = nullptr; ///< 玩家内存态管理器。
    RoomManager* room_manager = nullptr; ///< 房间管理器。
    LoginService* login_service = nullptr; ///< 登录服务。
    MatchService* match_service = nullptr; ///< 匹配服务。
    RankService* rank_service = nullptr; ///< 排行榜服务。
    SettlementService* settlement_service = nullptr; ///< 结算服务。
    GameLoop* game_loop = nullptr; ///< 固定 tick 游戏循环。
    MysqlClient* mysql_client = nullptr; ///< MySQL 客户端封装。
    RedisClient* redis_client = nullptr; ///< Redis 客户端封装。
    PlayerRepository* player_repository = nullptr; ///< 玩家仓储。
    BattleRepository* battle_repository = nullptr; ///< 战斗仓储。
};

} // namespace game
