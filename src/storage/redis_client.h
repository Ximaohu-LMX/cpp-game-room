#pragma once

#include <map>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#if defined(GAME_USE_REDIS) && GAME_USE_REDIS
#include <hiredis/hiredis.h>
#endif

namespace game {

class RedisClient {
public:
    explicit RedisClient(bool memory_mode = false);
    ~RedisClient();

    RedisClient(const RedisClient&) = delete;
    RedisClient& operator=(const RedisClient&) = delete;

    bool Connect();
    void Disconnect();

    bool ZAdd(const std::string& key, int score, const std::string& member);
    std::vector<std::pair<std::string, int>> ZRevRange(const std::string& key, int start, int stop);

    bool Set(const std::string& key, const std::string& value);
    std::string Get(const std::string& key);

private:
    std::recursive_mutex mutex_;
    bool memory_mode_ = false;
    std::unordered_map<std::string, std::string> kv_;
    std::unordered_map<std::string, std::unordered_map<std::string, int>> zsets_;
#if defined(GAME_USE_REDIS) && GAME_USE_REDIS
    redisContext* context_ = nullptr;
#endif
};

} // namespace game
