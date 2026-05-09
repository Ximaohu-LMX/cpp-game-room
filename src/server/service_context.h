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

class ServiceContext {
public:
    ConnectionManager* connection_manager = nullptr;
    PlayerManager* player_manager = nullptr;
    RoomManager* room_manager = nullptr;
    LoginService* login_service = nullptr;
    MatchService* match_service = nullptr;
    RankService* rank_service = nullptr;
    SettlementService* settlement_service = nullptr;
    GameLoop* game_loop = nullptr;
    MysqlClient* mysql_client = nullptr;
    RedisClient* redis_client = nullptr;
    PlayerRepository* player_repository = nullptr;
    BattleRepository* battle_repository = nullptr;
};

} // namespace game

