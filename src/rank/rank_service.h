#pragma once

#include "net/session.h"
#include "storage/redis_client.h"

#include <cstdint>
#include <string>
#include <vector>

namespace game {

class MessageDispatcher;

/**
 * @brief 排行榜条目。
 */
struct RankEntry {
    int64_t player_id = 0; ///< 玩家 ID。
    std::string name; ///< 玩家名。
    int score = 0; ///< 积分。
    int rank = 0; ///< 排名。
};

/**
 * @brief 排行榜服务。
 * @help 使用 Redis Sorted Set 维护 rank:score，member 为 player_id，score 为玩家积分。
 */
class RankService {
public:
    /**
     * @brief 创建排行榜服务。
     * @param redis Redis 客户端。
     */
    explicit RankService(RedisClient* redis = nullptr);

    /**
     * @brief 初始化排行榜服务。
     * @return 初始化成功返回 true，否则返回 false。
     */
    bool Init();

    /**
     * @brief 注册排行榜请求处理函数。
     * @param dispatcher 消息分发器。
     */
    void RegisterHandlers(MessageDispatcher& dispatcher);

    /**
     * @brief 更新玩家排行榜分数。
     * @param player_id 玩家 ID。
     * @param score 玩家最新积分。
     */
    void UpdateScore(int64_t player_id, int score);

    /**
     * @brief 查询排行榜。
     * @param offset 起始排名偏移。
     * @param limit 查询数量。
     * @return 排行榜条目列表。
     */
    std::vector<RankEntry> GetTopN(int offset, int limit);

    /**
     * @brief 处理排行榜请求。
     * @param session 当前会话。
     * @param packet 排行榜请求包。
     */
    void HandleRankRequest(const SessionPtr& session, const Packet& packet);

private:
    RedisClient* redis_;
};

} // namespace game
