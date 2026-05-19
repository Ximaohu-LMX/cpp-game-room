#pragma once

#include <cstdint>
#include <string>
#include <mutex>
#include <unordered_map>
#include <vector>

#if defined(GAME_USE_MYSQL) && GAME_USE_MYSQL
#include <mysql.h>
#endif

namespace game {

/** @brief MySQL 查询结果的一行，key 为字段名。 */
using QueryRow = std::unordered_map<std::string, std::string>;

/** @brief MySQL 查询结果集合。 */
using QueryResult = std::vector<QueryRow>;

/**
 * @brief MySQL 客户端封装。
 * @help 默认构建为内存 stub；开启 GAME_USE_MYSQL 后使用 MySQL/MariaDB C API。
 */
class MysqlClient {
public:
    MysqlClient() = default;

    /**
     * @brief 析构时断开连接。
     */
    ~MysqlClient();

    MysqlClient(const MysqlClient&) = delete;
    MysqlClient& operator=(const MysqlClient&) = delete;

    /**
     * @brief 建立 MySQL 连接。
     * @return 连接成功返回 true，否则返回 false。
     */
    bool Connect();

    /**
     * @brief 断开 MySQL 连接。
     */
    void Disconnect();

    /**
     * @brief 执行不关心影响行数的 SQL。
     * @param sql SQL 语句。
     * @return 执行成功返回 true，否则返回 false。
     */
    bool Execute(const std::string& sql);

    /**
     * @brief 执行 SQL 并返回影响行数。
     * @param sql SQL 语句。
     * @return 影响行数；执行失败返回 -1。
     */
    int64_t ExecuteAffected(const std::string& sql);

    /**
     * @brief 执行查询 SQL。
     * @param sql SQL 语句。
     * @return 查询结果。
     */
    QueryResult Query(const std::string& sql);

    /**
     * @brief 转义字符串，避免 SQL 字符串字段破坏语句。
     * @param value 原始字符串。
     * @return 转义后的字符串。
     */
    std::string EscapeString(const std::string& value);

    /**
     * @brief 判断是否已连接。
     * @return 已连接返回 true，否则返回 false。
     */
    bool IsConnected() const;

private:
    mutable std::recursive_mutex mutex_;
#if defined(GAME_USE_MYSQL) && GAME_USE_MYSQL
    MYSQL* conn_ = nullptr;
#else
    bool connected_ = false;
#endif
};

} // namespace game
