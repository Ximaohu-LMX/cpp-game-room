#pragma once

#include "net/session.h"

#include <cstdint>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace game {

/**
 * @brief 连接管理器。
 * @help 维护 session_id 到 Session、player_id 到 Session 的映射，支持按玩家查找在线连接。
 */
class ConnectionManager {
public:
    /**
     * @brief 添加会话。
     * @param session 新连接会话。
     */
    void Add(SessionPtr session) {
        if (!session) {
            return;
        }
        std::lock_guard<std::mutex> lock(mutex_);
        sessions_[session->SessionId()] = session;
        if (session->PlayerId() != 0) {
            player_sessions_[session->PlayerId()] = session;
        }
    }

    /**
     * @brief 绑定玩家 ID 到会话。
     * @param player_id 玩家 ID。
     * @param session 当前玩家对应会话。
     */
    void BindPlayer(int64_t player_id, const SessionPtr& session) {
        if (!session) {
            return;
        }
        std::lock_guard<std::mutex> lock(mutex_);
        player_sessions_[player_id] = session;
    }

    /**
     * @brief 移除会话。
     * @param session_id 会话 ID。
     */
    void Remove(uint64_t session_id) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = sessions_.find(session_id);
        if (it != sessions_.end() && it->second) {
            player_sessions_.erase(it->second->PlayerId());
        }
        sessions_.erase(session_id);
    }

    /**
     * @brief 根据 session_id 获取会话。
     * @param session_id 会话 ID。
     * @return 找到返回 Session，否则返回 nullptr。
     */
    SessionPtr Get(uint64_t session_id) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = sessions_.find(session_id);
        return it == sessions_.end() ? nullptr : it->second;
    }

    /**
     * @brief 根据玩家 ID 获取在线会话。
     * @param player_id 玩家 ID。
     * @return 在线返回 Session，否则返回 nullptr。
     */
    SessionPtr GetByPlayerId(int64_t player_id) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = player_sessions_.find(player_id);
        return it == player_sessions_.end() ? nullptr : it->second;
    }

    /**
     * @brief 获取当前所有会话快照。
     * @return Session 列表。
     */
    std::vector<SessionPtr> AllSessions() {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<SessionPtr> result;
        result.reserve(sessions_.size());
        for (const auto& [_, session] : sessions_) {
            result.push_back(session);
        }
        return result;
    }

private:
    std::mutex mutex_;
    std::unordered_map<uint64_t, SessionPtr> sessions_;
    std::unordered_map<int64_t, SessionPtr> player_sessions_;
};

} // namespace game
