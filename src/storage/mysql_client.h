#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <mutex>
#include <unordered_map>
#include <vector>
#include <mysql.h>

namespace game {

/** @brief MySQL 查询结果的一行，key 为字段名。 */
using QueryRow = std::unordered_map<std::string, std::string>;

/** @brief MySQL 查询结果集合。 */
using QueryResult = std::vector<QueryRow>;

/**
 * @brief MySQL 客户端封装。
 * @help 使用 MySQL/MariaDB C API 连接真实数据库。
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

    /** @brief 执行自增 INSERT，返回生成的 ID；失败返回 0。 */
    int64_t ExecuteInsert(const std::string& sql);

    /**
     * @brief 在同一连接、同一事务内执行回调；回调失败或抛异常时回滚。
     * @help 整个事务持有连接锁，防止其他线程的 SQL 混入事务；不支持嵌套事务。
     */
    bool RunTransaction(const std::function<bool()>& operation);

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
    void ResetOnConnectionError();

    mutable std::recursive_mutex mutex_;
    MYSQL* conn_ = nullptr;
    bool in_transaction_ = false;
};

} // namespace game
