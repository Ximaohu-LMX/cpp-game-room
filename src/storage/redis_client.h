#pragma once

#include <map>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace game {

class RedisClient {
public:
    bool Connect();

    bool ZAdd(const std::string& key, int score, const std::string& member);
    std::vector<std::pair<std::string, int>> ZRevRange(const std::string& key, int start, int stop);

    bool Set(const std::string& key, const std::string& value);
    std::string Get(const std::string& key);

private:
    std::mutex mutex_;
    std::unordered_map<std::string, std::string> kv_;
    std::unordered_map<std::string, std::unordered_map<std::string, int>> zsets_;
};

} // namespace game

