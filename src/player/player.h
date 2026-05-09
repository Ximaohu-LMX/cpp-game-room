#pragma once

#include <cstdint>
#include <memory>
#include <string>

namespace game {

enum class PlayerStatus {
    Offline,
    Online,
    Matching,
    InRoom,
    Playing
};

class Player {
public:
    Player(int64_t player_id, std::string name, int score = 1000);

    int64_t Id() const;
    const std::string& Name() const;

    void SetStatus(PlayerStatus status);
    PlayerStatus Status() const;

    void SetScore(int score);
    int Score() const;

    void SetRoomId(int64_t room_id);
    int64_t RoomId() const;

private:
    int64_t player_id_;
    std::string name_;
    int score_;
    PlayerStatus status_ = PlayerStatus::Offline;
    int64_t room_id_ = 0;
};

using PlayerPtr = std::shared_ptr<Player>;

} // namespace game

