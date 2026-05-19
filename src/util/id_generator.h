#pragma once

#include <atomic>
#include <cstdint>

namespace game {

/**
 * @brief 简单递增 ID 生成器。
 * @help 当前使用进程内 atomic 递增，适合单进程示例；分布式环境需要替换为全局唯一 ID 方案。
 */
class IdGenerator {
public:
    /**
     * @brief 获取下一个玩家 ID。
     * @return 玩家 ID。
     */
    static int64_t NextPlayerId() {
        static std::atomic<int64_t> id{100000};
        return id.fetch_add(1);
    }

    /**
     * @brief 获取下一个房间 ID。
     * @return 房间 ID。
     */
    static int64_t NextRoomId() {
        static std::atomic<int64_t> id{1000};
        return id.fetch_add(1);
    }

    /**
     * @brief 获取下一个战斗 ID。
     * @return 战斗 ID。
     */
    static int64_t NextBattleId() {
        static std::atomic<int64_t> id{1000000};
        return id.fetch_add(1);
    }

    /**
     * @brief 获取下一个会话 ID。
     * @return 会话 ID。
     */
    static int64_t NextSessionId() {
        static std::atomic<int64_t> id{1};
        return id.fetch_add(1);
    }
};

} // namespace game
