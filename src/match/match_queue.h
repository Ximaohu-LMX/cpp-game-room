#pragma once

#include <cstdint>
#include <cstddef>
#include <deque>
#include <mutex>
#include <unordered_set>
#include <vector>

namespace game {

/**
 * @brief 匹配队列。
 * @help 使用 deque 保证入队顺序，使用 unordered_set 防止重复入队。
 */
class MatchQueue {
public:
    /**
     * @brief 将玩家加入匹配队列。
     * @param player_id 玩家 ID。
     * @return 入队成功返回 true；已在队列中返回 false。
     */
    bool Push(int64_t player_id);

    /**
     * @brief 从匹配队列移除玩家。
     * @param player_id 玩家 ID。
     * @return 移除成功返回 true；玩家不在队列中返回 false。
     */
    bool Remove(int64_t player_id);

    /**
     * @brief 判断玩家是否正在匹配。
     * @param player_id 玩家 ID。
     * @return 在队列中返回 true，否则返回 false。
     */
    bool Contains(int64_t player_id) const;

    /**
     * @brief 按入队顺序弹出 N 个玩家。
     * @param n 需要弹出的玩家数量。
     * @return 实际弹出的玩家 ID 列表。
     */
    std::vector<int64_t> PopN(size_t n);

    /**
     * @brief 获取当前队列长度。
     * @return 队列中玩家数量。
     */
    size_t Size() const;

private:
    mutable std::mutex mutex_;
    std::deque<int64_t> queue_;
    std::unordered_set<int64_t> in_queue_;
};

} // namespace game
