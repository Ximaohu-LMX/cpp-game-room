#include "storage/redis_client.h"

#include "config/config_manager.h"
#include "util/logger.h"

#include <cstdlib>

namespace game {

RedisClient::~RedisClient() {
    Disconnect();
}

bool RedisClient::Connect() {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    Disconnect();

    const auto config = ConfigManager::Instance().Redis();
    context_ = redisConnect(config.host.c_str(), config.port);
    if (!context_ || context_->err) {
        LOG_ERROR("redis connect failed: {}", context_ ? context_->errstr : "null context");
        Disconnect();
        return false;
    }
    LOG_INFO("redis connected to {}:{}", config.host, config.port);
    return true;
}

void RedisClient::Disconnect() {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (context_) {
        redisFree(context_);
        context_ = nullptr;
    }
}

bool RedisClient::ZAdd(const std::string& key, int score, const std::string& member) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (!context_ && !Connect()) {
        return false;
    }
    auto* reply = static_cast<redisReply*>(
        redisCommand(context_, "ZADD %s %d %s", key.c_str(), score, member.c_str()));
    if (!reply) {
        LOG_ERROR("redis ZADD failed: {}", context_ ? context_->errstr : "null reply");
        Disconnect();
        return false;
    }
    const bool ok = reply->type != REDIS_REPLY_ERROR;
    if (!ok) {
        LOG_ERROR("redis ZADD error: {}", reply->str ? reply->str : "");
    }
    freeReplyObject(reply);
    return ok;
}

std::vector<std::pair<std::string, int>> RedisClient::ZRevRange(const std::string& key, int start, int stop) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    std::vector<std::pair<std::string, int>> items;
    if (!context_ && !Connect()) {
        return items;
    }
    auto* reply = static_cast<redisReply*>(
        redisCommand(context_, "ZREVRANGE %s %d %d WITHSCORES", key.c_str(), start, stop));
    if (!reply) {
        LOG_ERROR("redis ZREVRANGE failed: {}", context_ ? context_->errstr : "null reply");
        Disconnect();
        return items;
    }
    if (reply->type != REDIS_REPLY_ARRAY) {
        LOG_ERROR("redis ZREVRANGE unexpected reply type {}", reply->type);
        freeReplyObject(reply);
        return items;
    }

    for (size_t i = 0; i + 1 < reply->elements; i += 2) {
        auto* member = reply->element[i];
        auto* score = reply->element[i + 1];
        if (!member || !score || !member->str || !score->str) {
            continue;
        }
        items.emplace_back(std::string(member->str, member->len), std::atoi(score->str));
    }
    freeReplyObject(reply);
    return items;
}

bool RedisClient::Set(const std::string& key, const std::string& value) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (!context_ && !Connect()) {
        return false;
    }
    auto* reply = static_cast<redisReply*>(
        redisCommand(context_, "SET %s %s", key.c_str(), value.c_str()));
    if (!reply) {
        LOG_ERROR("redis SET failed: {}", context_ ? context_->errstr : "null reply");
        Disconnect();
        return false;
    }
    const bool ok = reply->type != REDIS_REPLY_ERROR;
    if (!ok) {
        LOG_ERROR("redis SET error: {}", reply->str ? reply->str : "");
    }
    freeReplyObject(reply);
    return ok;
}

std::string RedisClient::Get(const std::string& key) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (!context_ && !Connect()) {
        return "";
    }
    auto* reply = static_cast<redisReply*>(
        redisCommand(context_, "GET %s", key.c_str()));
    if (!reply) {
        LOG_ERROR("redis GET failed: {}", context_ ? context_->errstr : "null reply");
        Disconnect();
        return "";
    }
    std::string value;
    if (reply->type == REDIS_REPLY_STRING && reply->str) {
        value.assign(reply->str, reply->len);
    } else if (reply->type == REDIS_REPLY_ERROR) {
        LOG_ERROR("redis GET error: {}", reply->str ? reply->str : "");
    }
    freeReplyObject(reply);
    return value;
}

} // namespace game
