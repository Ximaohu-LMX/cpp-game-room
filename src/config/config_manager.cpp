#include "config/config_manager.h"

#include <fstream>
#include <sstream>

namespace game {

namespace {

std::string Trim(std::string value) {
    while (!value.empty() && (value.front() == ' ' || value.front() == '\t' || value.front() == '"')) {
        value.erase(value.begin());
    }
    while (!value.empty() && (value.back() == ' ' || value.back() == '\t' || value.back() == '\r' || value.back() == '"')) {
        value.pop_back();
    }
    return value;
}

bool SplitKeyValue(const std::string& line, std::string* key, std::string* value) {
    const auto pos = line.find(':');
    if (pos == std::string::npos) {
        return false;
    }
    *key = Trim(line.substr(0, pos));
    *value = Trim(line.substr(pos + 1));
    return !key->empty();
}

} // namespace

ConfigManager& ConfigManager::Instance() {
    static ConfigManager instance;
    return instance;
}

bool ConfigManager::Load(const std::string& path) {
    std::ifstream input(path);
    if (!input.is_open()) {
        return false;
    }

    std::string section;
    std::string line;
    while (std::getline(input, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }
        if (line[0] != ' ' && line.back() == ':') {
            section = Trim(line.substr(0, line.size() - 1));
            continue;
        }

        std::string key;
        std::string value;
        if (!SplitKeyValue(line, &key, &value) || value.empty()) {
            continue;
        }

        if (section == "server") {
            if (key == "host") server_host_ = value;
            if (key == "port") server_port_ = std::stoi(value);
            if (key == "worker_threads") worker_threads_ = std::stoi(value);
            if (key == "tick_interval_ms") tick_interval_ms_ = std::stoi(value);
        } else if (section == "mysql") {
            if (key == "host") mysql_.host = value;
            if (key == "port") mysql_.port = std::stoi(value);
            if (key == "user") mysql_.user = value;
            if (key == "password") mysql_.password = value;
            if (key == "database") mysql_.database = value;
        } else if (section == "redis") {
            if (key == "host") redis_.host = value;
            if (key == "port") redis_.port = std::stoi(value);
        } else if (section == "match") {
            if (key == "room_player_count") room_player_count_ = std::stoi(value);
        }
    }
    return true;
}

std::string ConfigManager::ServerHost() const { return server_host_; }
int ConfigManager::ServerPort() const { return server_port_; }
int ConfigManager::WorkerThreads() const { return worker_threads_; }
int ConfigManager::TickIntervalMs() const { return tick_interval_ms_; }
int ConfigManager::RoomPlayerCount() const { return room_player_count_; }
MysqlConfig ConfigManager::Mysql() const { return mysql_; }
RedisConfig ConfigManager::Redis() const { return redis_; }

} // namespace game

