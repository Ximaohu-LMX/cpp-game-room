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

using QueryRow = std::unordered_map<std::string, std::string>;
using QueryResult = std::vector<QueryRow>;

class MysqlClient {
public:
    MysqlClient() = default;
    ~MysqlClient();

    MysqlClient(const MysqlClient&) = delete;
    MysqlClient& operator=(const MysqlClient&) = delete;

    bool Connect();
    void Disconnect();
    bool Execute(const std::string& sql);
    int64_t ExecuteAffected(const std::string& sql);
    QueryResult Query(const std::string& sql);
    std::string EscapeString(const std::string& value);
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
