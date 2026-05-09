#include "game/state_sync.h"

namespace game {

proto::GameStateNotify StateSync::BuildNotify(int64_t room_id, const GameState& state) {
    proto::GameStateNotify notify;
    notify.set_room_id(room_id);
    notify.set_frame_id(state.frame_id);
    for (const auto& [_, entity] : state.players) {
        auto* player = notify.add_players();
        player->set_player_id(entity.player_id);
        player->set_x(entity.x);
        player->set_y(entity.y);
        player->set_hp(entity.hp);
    }
    return notify;
}

} // namespace game

