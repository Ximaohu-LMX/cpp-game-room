#pragma once

#include <string>

namespace game {

struct MysqlConfig {
    std::string host = "127.0.0.1";
    int port = 3306;
    std::string user = "root";
    std::string password = "123456";
    std::string database = "game";
};

struct RedisConfig {
    std::string host = "127.0.0.1";
    int port = 6379;
};

class ConfigManager {
public:
    static ConfigManager& Instance();

    bool Load(const std::string& path);

    std::string ServerHost() const;
    int ServerPort() const;
    int WorkerThreads() const;
    int TickIntervalMs() const;
    int RoomPlayerCount() const;

    MysqlConfig Mysql() const;
    RedisConfig Redis() const;

private:
    std::string server_host_ = "0.0.0.0";
    int server_port_ = 9000;
    int worker_threads_ = 4;
    int tick_interval_ms_ = 50;
    int room_player_count_ = 2;
    MysqlConfig mysql_;
    RedisConfig redis_;
};

} // namespace game

