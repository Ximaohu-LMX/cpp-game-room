#include "player/player_manager.h"

namespace game {

PlayerPtr PlayerManager::GetOrCreatePlayer(int64_t player_id, const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = players_.find(player_id);
    if (it != players_.end()) {
        return it->second;
    }

    auto player = std::make_shared<Player>(player_id, name.empty() ? "player_" + std::to_string(player_id) : name);
    players_[player_id] = player;
    return player;
}

PlayerPtr PlayerManager::GetPlayer(int64_t player_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = players_.find(player_id);
    return it == players_.end() ? nullptr : it->second;
}

void PlayerManager::SetOnline(int64_t player_id, SessionPtr session) {
    std::lock_guard<std::mutex> lock(mutex_);
    online_sessions_[player_id] = std::move(session);
    auto it = players_.find(player_id);
    if (it != players_.end()) {
        it->second->SetStatus(PlayerStatus::Online);
    }
}

void PlayerManager::SetOffline(int64_t player_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    online_sessions_.erase(player_id);
    auto it = players_.find(player_id);
    if (it != players_.end()) {
        it->second->SetStatus(PlayerStatus::Offline);
    }
}

bool PlayerManager::IsOnline(int64_t player_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    return online_sessions_.find(player_id) != online_sessions_.end();
}

} // namespace game

