#pragma once

#include "net/session.h"
#include "player/player.h"

#include <cstdint>
#include <mutex>
#include <unordered_map>

namespace game {

/**
 * @brief 玩家管理器。
 * @help 维护玩家运行时对象和在线 Session 映射。
 */
class PlayerManager {
public:
    /**
     * @brief 获取或创建玩家对象。
     * @param player_id 玩家 ID。
     * @param name 玩家名，新建时使用。
     * @return 玩家对象。
     */
    PlayerPtr GetOrCreatePlayer(int64_t player_id, const std::string& name = "");

    /**
     * @brief 获取玩家对象。
     * @param player_id 玩家 ID。
     * @return 找到返回 Player，否则返回 nullptr。
     */
    PlayerPtr GetPlayer(int64_t player_id);

    /**
     * @brief 标记玩家在线。
     * @param player_id 玩家 ID。
     * @param session 当前在线会话。
     */
    void SetOnline(int64_t player_id, SessionPtr session);

    /**
     * @brief 标记玩家离线。
     * @param player_id 玩家 ID。
     */
    void SetOffline(int64_t player_id);

    /**
     * @brief 判断玩家是否在线。
     * @param player_id 玩家 ID。
     * @return 在线返回 true，否则返回 false。
     */
    bool IsOnline(int64_t player_id);

private:
    std::mutex mutex_;
    std::unordered_map<int64_t, PlayerPtr> players_;
    std::unordered_map<int64_t, SessionPtr> online_sessions_;
};

} // namespace game
