#include "storage/mysql_client.h"

#include "config/config_manager.h"
#include "util/logger.h"

#include <utility>

namespace game {

MysqlClient::~MysqlClient() {
    Disconnect();
}

bool MysqlClient::Connect() {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
#if defined(GAME_USE_MYSQL) && GAME_USE_MYSQL
    Disconnect();

    conn_ = mysql_init(nullptr);
    if (!conn_) {
        LOG_ERROR("mysql_init failed");
        return false;
    }

    const auto config = ConfigManager::Instance().Mysql();
    mysql_options(conn_, MYSQL_SET_CHARSET_NAME, "utf8mb4");
    if (!mysql_real_connect(conn_,
                            config.host.c_str(),
                            config.user.c_str(),
                            config.password.c_str(),
                            config.database.c_str(),
                            static_cast<unsigned int>(config.port),
                            nullptr,
                            CLIENT_MULTI_STATEMENTS)) {
        LOG_ERROR("mysql connect failed: {}", mysql_error(conn_));
        Disconnect();
        return false;
    }

    LOG_INFO("mysql connected to {}:{}/{}", config.host, config.port, config.database);
    return true;
#else
    LOG_INFO("mysql client uses in-memory stub in this version");
    connected_ = true;
    return true;
#endif
}

void MysqlClient::Disconnect() {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
#if defined(GAME_USE_MYSQL) && GAME_USE_MYSQL
    if (conn_) {
        mysql_close(conn_);
        conn_ = nullptr;
    }
#else
    connected_ = false;
#endif
}

bool MysqlClient::Execute(const std::string& sql) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
#if defined(GAME_USE_MYSQL) && GAME_USE_MYSQL
    if (!conn_ && !Connect()) {
        return false;
    }
    if (mysql_query(conn_, sql.c_str()) != 0) {
        LOG_ERROR("mysql execute failed: {}, sql={}", mysql_error(conn_), sql);
        return false;
    }

    while (mysql_next_result(conn_) == 0) {
        MYSQL_RES* result = mysql_store_result(conn_);
        if (result) {
            mysql_free_result(result);
        }
    }
    return true;
#else
    LOG_DEBUG("mysql execute: {}", sql);
    return connected_;
#endif
}

int64_t MysqlClient::ExecuteAffected(const std::string& sql) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
#if defined(GAME_USE_MYSQL) && GAME_USE_MYSQL
    if (!conn_ && !Connect()) {
        return -1;
    }
    if (mysql_query(conn_, sql.c_str()) != 0) {
        LOG_ERROR("mysql execute failed: {}, sql={}", mysql_error(conn_), sql);
        return -1;
    }
    return static_cast<int64_t>(mysql_affected_rows(conn_));
#else
    LOG_DEBUG("mysql execute affected: {}", sql);
    return connected_ ? 1 : -1;
#endif
}

QueryResult MysqlClient::Query(const std::string& sql) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
#if defined(GAME_USE_MYSQL) && GAME_USE_MYSQL
    QueryResult output;
    if (!conn_ && !Connect()) {
        return output;
    }
    if (mysql_query(conn_, sql.c_str()) != 0) {
        LOG_ERROR("mysql query failed: {}, sql={}", mysql_error(conn_), sql);
        return output;
    }

    MYSQL_RES* result = mysql_store_result(conn_);
    if (!result) {
        if (mysql_field_count(conn_) != 0) {
            LOG_ERROR("mysql store result failed: {}", mysql_error(conn_));
        }
        return output;
    }

    const unsigned int field_count = mysql_num_fields(result);
    MYSQL_FIELD* fields = mysql_fetch_fields(result);
    MYSQL_ROW row = nullptr;
    while ((row = mysql_fetch_row(result)) != nullptr) {
        unsigned long* lengths = mysql_fetch_lengths(result);
        QueryRow item;
        for (unsigned int i = 0; i < field_count; ++i) {
            const char* name = fields[i].name ? fields[i].name : "";
            item[name] = row[i] ? std::string(row[i], lengths[i]) : "";
        }
        output.push_back(std::move(item));
    }
    mysql_free_result(result);
    return output;
#else
    LOG_DEBUG("mysql query: {}", sql);
    return {};
#endif
}

std::string MysqlClient::EscapeString(const std::string& value) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
#if defined(GAME_USE_MYSQL) && GAME_USE_MYSQL
    if (!conn_ && !Connect()) {
        return value;
    }
    std::string escaped;
    escaped.resize(value.size() * 2 + 1);
    const unsigned long len = mysql_real_escape_string(
        conn_, escaped.data(), value.data(), static_cast<unsigned long>(value.size()));
    escaped.resize(len);
    return escaped;
#else
    std::string escaped;
    escaped.reserve(value.size());
    for (char ch : value) {
        if (ch == '\'' || ch == '\\') {
            escaped.push_back('\\');
        }
        escaped.push_back(ch);
    }
    return escaped;
#endif
}

bool MysqlClient::IsConnected() const {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
#if defined(GAME_USE_MYSQL) && GAME_USE_MYSQL
    return conn_ != nullptr;
#else
    return connected_;
#endif
}

} // namespace game
