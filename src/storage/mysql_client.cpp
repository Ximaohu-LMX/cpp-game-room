#include "storage/mysql_client.h"

#include "util/logger.h"

namespace game {

bool MysqlClient::Connect() {
    LOG_INFO("mysql client uses in-memory stub in this version");
    return true;
}

bool MysqlClient::Execute(const std::string& sql) {
    LOG_DEBUG("mysql execute: {}", sql);
    return true;
}

QueryResult MysqlClient::Query(const std::string& sql) {
    LOG_DEBUG("mysql query: {}", sql);
    return {};
}

} // namespace game

