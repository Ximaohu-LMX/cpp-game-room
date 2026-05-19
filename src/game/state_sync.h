#pragma once

#include "game/game_state.h"

#include "game.pb.h"

namespace game {

/**
 * @brief 游戏状态同步工具。
 * @help 将服务端内部 GameState 转换为 protobuf 通知，供房间广播给客户端。
 */
class StateSync {
public:
    /**
     * @brief 构建状态同步通知。
     * @param room_id 房间 ID。
     * @param state 当前游戏状态。
     * @return protobuf GameStateNotify。
     */
    static proto::GameStateNotify BuildNotify(int64_t room_id, const GameState& state);
};

} // namespace game
