#include "player/player.h"

#include <utility>

namespace game {

Player::Player(int64_t player_id, std::string name, int score)
    : player_id_(player_id), name_(std::move(name)), score_(score) {}

int64_t Player::Id() const { return player_id_; }
const std::string& Player::Name() const { return name_; }
void Player::SetStatus(PlayerStatus status) { status_ = status; }
PlayerStatus Player::Status() const { return status_; }
void Player::SetScore(int score) { score_ = score; }
int Player::Score() const { return score_; }
void Player::SetRoomId(int64_t room_id) { room_id_ = room_id; }
int64_t Player::RoomId() const { return room_id_; }

} // namespace game

