#include "storage/redis_client.h"

#include <algorithm>

namespace game {

bool RedisClient::Connect() {
    return true;
}

bool RedisClient::ZAdd(const std::string& key, int score, const std::string& member) {
    std::lock_guard<std::mutex> lock(mutex_);
    zsets_[key][member] = score;
    return true;
}

std::vector<std::pair<std::string, int>> RedisClient::ZRevRange(const std::string& key, int start, int stop) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::pair<std::string, int>> items;
    auto it = zsets_.find(key);
    if (it == zsets_.end()) {
        return items;
    }

    for (const auto& [member, score] : it->second) {
        items.emplace_back(member, score);
    }
    std::sort(items.begin(), items.end(), [](const auto& a, const auto& b) {
        if (a.second != b.second) {
            return a.second > b.second;
        }
        return a.first < b.first;
    });

    if (start < 0) start = 0;
    if (stop >= static_cast<int>(items.size())) stop = static_cast<int>(items.size()) - 1;
    if (start > stop || start >= static_cast<int>(items.size())) {
        return {};
    }
    return {items.begin() + start, items.begin() + stop + 1};
}

bool RedisClient::Set(const std::string& key, const std::string& value) {
    std::lock_guard<std::mutex> lock(mutex_);
    kv_[key] = value;
    return true;
}

std::string RedisClient::Get(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = kv_.find(key);
    return it == kv_.end() ? "" : it->second;
}

} // namespace game

