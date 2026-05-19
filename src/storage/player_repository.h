#pragma once

#include "storage/mysql_client.h"

#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>

namespace game {

/**
 * @brief 玩家持久化数据。
 */
struct PlayerData {
    int64_t player_id = 0; ///< 玩家 ID。
    std::string name; ///< 玩家名。
    int score = 1000; ///< 当前积分。
    int win_count = 0; ///< 累计胜场。
    int lose_count = 0; ///< 累计败场。
};

/**
 * @brief 玩家仓储。
 * @help 封装 player 表读写；默认模式使用内存 map，真实模式读写 MySQL。
 */
class PlayerRepository {
public:
    /**
     * @brief 创建玩家仓储。
     * @param mysql MySQL 客户端；为空时使用内存模式。
     */
    explicit PlayerRepository(MysqlClient* mysql = nullptr);

    /**
     * @brief 加载玩家数据。
     * @param player_id 玩家 ID。
     * @return 玩家数据；不存在时会创建默认数据。
     */
    PlayerData LoadPlayer(int64_t player_id);

    /**
     * @brief 创建玩家。
     * @param data 玩家数据。
     * @return 创建成功返回 true，否则返回 false。
     */
    bool CreatePlayer(const PlayerData& data);

    /**
     * @brief 更新玩家积分和胜负统计。
     * @param player_id 玩家 ID。
     * @param score_delta 积分变化，正数记胜场，负数记败场。
     * @return 更新成功返回 true，否则返回 false。
     */
    bool UpdatePlayerScore(int64_t player_id, int score_delta);

private:
    MysqlClient* mysql_;
    std::mutex mutex_;
    std::unordered_map<int64_t, PlayerData> players_;
};

} // namespace game
