#include "storage/mysql_client.h"

#include "config/config_manager.h"
#include "util/logger.h"

#include <utility>
#include <errmsg.h>

namespace game {

MysqlClient::~MysqlClient() {
    Disconnect();
}

bool MysqlClient::Connect() {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (in_transaction_) {
        return false;
    }
    Disconnect();  // 先断开旧连接，防止重复连接导致资源泄漏

    conn_ = mysql_init(nullptr);
    if (!conn_) {
        LOG_ERROR("mysql_init failed");
        return false;
    }

    const auto config = ConfigManager::Instance().Mysql();
    mysql_options(conn_, MYSQL_SET_CHARSET_NAME, "utf8mb4");
    const bool reconnect = false;
    mysql_options(conn_, MYSQL_OPT_RECONNECT, &reconnect);
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

void MysqlClient::ResetOnConnectionError() {
    if (!conn_ || in_transaction_) {
        return;
    }
    const auto error = mysql_errno(conn_);
    if (error == CR_SERVER_GONE_ERROR || error == CR_SERVER_LOST) {
        // 事务外的分配 ID / 查询失败也要丢弃坏连接，下一次请求才能重新连接。
        Disconnect();
    }
}

bool MysqlClient::Execute(const std::string& sql) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (!conn_ && !Connect()) {
        return false;
    }
    if (mysql_query(conn_, sql.c_str()) != 0) {
        LOG_ERROR("mysql execute failed: {}, sql={}", mysql_error(conn_), sql);
        ResetOnConnectionError();
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
        ResetOnConnectionError();
        return -1;
    }
    return static_cast<int64_t>(mysql_affected_rows(conn_));
}

int64_t MysqlClient::ExecuteInsert(const std::string& sql) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (ExecuteAffected(sql) != 1) {
        return 0;
    }
    // INSERT 与读取 ID 必须在同一临界区，不能被其他线程的 INSERT 打断。
    return static_cast<int64_t>(mysql_insert_id(conn_));
}

bool MysqlClient::RunTransaction(const std::function<bool()>& operation) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (in_transaction_ || !operation || (!conn_ && !Connect())) {
        return false;
    }
    if (mysql_query(conn_, "START TRANSACTION") != 0) {
        LOG_ERROR("mysql begin transaction failed: {}", mysql_error(conn_));
        Disconnect();
        return false;
    }
    in_transaction_ = true;
    try {
        const bool success = operation();
        if (!conn_) {
            in_transaction_ = false;
            return false;
        }
        if (!success) {
            if (mysql_rollback(conn_) != 0) {
                Disconnect();
            }
            in_transaction_ = false;
            return false;
        }
        if (mysql_commit(conn_) != 0) {
            // COMMIT 的响应丢失时结果可能未知；关闭连接，让调用方复用原 ID 重试。
            LOG_ERROR("mysql commit failed: {}", mysql_error(conn_));
            Disconnect();
            in_transaction_ = false;
            return false;
        }
        in_transaction_ = false;
        return true;
    } catch (...) {
        if (conn_ && mysql_rollback(conn_) != 0) {
            Disconnect();
        }
        in_transaction_ = false;
        throw;
    }
}

QueryResult MysqlClient::Query(const std::string& sql) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    QueryResult output;
    if (!conn_ && !Connect()) {
        return output;
    }
    if (mysql_query(conn_, sql.c_str()) != 0) {
        LOG_ERROR("mysql query failed: {}, sql={}", mysql_error(conn_), sql);
        ResetOnConnectionError();
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
