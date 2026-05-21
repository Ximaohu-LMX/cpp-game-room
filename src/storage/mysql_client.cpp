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
    Disconnect();  // 先断开旧连接，防止重复连接导致资源泄漏

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
}

void MysqlClient::Disconnect() {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (conn_) {
        mysql_close(conn_);
        conn_ = nullptr;
    }
}

bool MysqlClient::Execute(const std::string& sql) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
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
}

int64_t MysqlClient::ExecuteAffected(const std::string& sql) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (!conn_ && !Connect()) {
        return -1;
    }
    if (mysql_query(conn_, sql.c_str()) != 0) {
        LOG_ERROR("mysql execute failed: {}, sql={}", mysql_error(conn_), sql);
        return -1;
    }
    return static_cast<int64_t>(mysql_affected_rows(conn_));
}

QueryResult MysqlClient::Query(const std::string& sql) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
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
}

std::string MysqlClient::EscapeString(const std::string& value) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (!conn_ && !Connect()) {
        return value;
    }
    std::string escaped;
    escaped.resize(value.size() * 2 + 1);
    const unsigned long len = mysql_real_escape_string(
        conn_, escaped.data(), value.data(), static_cast<unsigned long>(value.size()));
    escaped.resize(len);
    return escaped;
}

bool MysqlClient::IsConnected() const {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    return conn_ != nullptr;
}

} // namespace game
