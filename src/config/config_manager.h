#pragma once

#include <string>

namespace game {

/**
 * @brief MySQL 连接配置。
 */
struct MysqlConfig {
    std::string host = "127.0.0.1"; ///< 主机地址。
    int port = 3306; ///< 端口。
    std::string user = "root"; ///< 用户名。
    std::string password = "123456"; ///< 密码。
    std::string database = "game"; ///< 数据库名。
};

/**
 * @brief Redis 连接配置。
 */
struct RedisConfig {
    std::string host = "127.0.0.1"; ///< 主机地址。
    int port = 6379; ///< 端口。
};

/**
 * @brief 配置管理器。
 * @help 简单解析 config/server.yaml，并以单例形式提供全局配置读取。
 */
class ConfigManager {
public:
    /**
     * @brief 获取配置管理器单例。
     * @return ConfigManager 实例。
     */
    static ConfigManager& Instance();

    /**
     * @brief 加载配置文件。
     * @param path 配置文件路径。
     * @return 加载成功返回 true，否则返回 false。
     */
    bool Load(const std::string& path);

    /** @brief 获取服务监听地址。 @return host 字符串。 */
    std::string ServerHost() const;
    /** @brief 获取服务监听端口。 @return 端口号。 */
    int ServerPort() const;
    /** @brief 获取网络 worker 线程数。 @return 线程数。 */
    int WorkerThreads() const;
    /** @brief 获取 tick 间隔。 @return tick 间隔毫秒数。 */
    int TickIntervalMs() const;
    /** @brief 获取单房间玩家数。 @return 匹配成房所需人数。 */
    int RoomPlayerCount() const;

    /** @brief 获取 MySQL 配置。 @return MysqlConfig。 */
    MysqlConfig Mysql() const;
    /** @brief 获取 Redis 配置。 @return RedisConfig。 */
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
