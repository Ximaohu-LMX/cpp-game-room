#pragma once

#include "net/session.h"
#include "player/player.h"

#include <cstdint>
#include <mutex>
#include <unordered_map>

namespace game {

class PlayerManager {
public:
    PlayerPtr GetOrCreatePlayer(int64_t player_id, const std::string& name = "");
    PlayerPtr GetPlayer(int64_t player_id);

    void SetOnline(int64_t player_id, SessionPtr session);
    void SetOffline(int64_t player_id);

    bool IsOnline(int64_t player_id);

private:
    std::mutex mutex_;
    std::unordered_map<int64_t, PlayerPtr> players_;
    std::unordered_map<int64_t, SessionPtr> online_sessions_;
};

} // namespace game

