#pragma once

#include <mutex>
#include <string>
#include <utility>
#include <vector>
#include <hiredis/hiredis.h>

namespace game {

/**
 * @brief Redis 客户端封装。
 * @help 使用 hiredis 连接真实 Redis。
 */
class RedisClient {
public:
    RedisClient() = default;

    /**
     * @brief 析构时断开连接。
     */
    ~RedisClient();

    RedisClient(const RedisClient&) = delete;
    RedisClient& operator=(const RedisClient&) = delete;

    /**
     * @brief 建立 Redis 连接。
     * @return 连接成功返回 true，否则返回 false。
     */
    bool Connect();

    /**
     * @brief 断开 Redis 连接。
     */
    void Disconnect();

    /**
     * @brief 写入 Sorted Set 分数。
     * @param key Redis key。
     * @param score 分数。
     * @param member 成员。
     * @return 写入成功返回 true，否则返回 false。
     */
    bool ZAdd(const std::string& key, int score, const std::string& member);

    /**
     * @brief 按分数从高到低查询 Sorted Set。
     * @param key Redis key。
     * @param start 起始下标。
     * @param stop 结束下标。
     * @return member-score 列表。
     */
    std::vector<std::pair<std::string, int>> ZRevRange(const std::string& key, int start, int stop);

    /**
     * @brief 写入字符串 KV。
     * @param key Redis key。
     * @param value 字符串值。
     * @return 写入成功返回 true，否则返回 false。
     */
    bool Set(const std::string& key, const std::string& value);

    /**
     * @brief 读取字符串 KV。
     * @param key Redis key。
     * @return key 对应字符串，不存在返回空字符串。
     */
    std::string Get(const std::string& key);

private:
    std::recursive_mutex mutex_;
    redisContext* context_ = nullptr;
};

} // namespace game
