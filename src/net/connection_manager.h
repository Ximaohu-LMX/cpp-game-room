#pragma once

#include "net/session.h"

#include <cstdint>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace game {

class ConnectionManager {
public:
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

    void BindPlayer(int64_t player_id, const SessionPtr& session) {
        if (!session) {
            return;
        }
        std::lock_guard<std::mutex> lock(mutex_);
        player_sessions_[player_id] = session;
    }

    void Remove(uint64_t session_id) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = sessions_.find(session_id);
        if (it != sessions_.end() && it->second) {
            player_sessions_.erase(it->second->PlayerId());
        }
        sessions_.erase(session_id);
    }

    SessionPtr Get(uint64_t session_id) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = sessions_.find(session_id);
        return it == sessions_.end() ? nullptr : it->second;
    }

    SessionPtr GetByPlayerId(int64_t player_id) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = player_sessions_.find(player_id);
        return it == player_sessions_.end() ? nullptr : it->second;
    }

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

