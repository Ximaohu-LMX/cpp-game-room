#pragma once

#include "game/game_state.h"
#include "game/input_buffer.h"

#include <cstdint>
#include <mutex>
#include <vector>

namespace game {

/**
 * @brief 单局战斗逻辑房间。
 * @help 负责消费玩家输入、推进实体状态、判断游戏是否结束；不负责网络广播和数据库结算。
 */
class GameRoom {
public:
    /**
     * @brief 创建战斗房间。
     * @param room_id 房间 ID。
     */
    explicit GameRoom(int64_t room_id);

    /**
     * @brief 获取房间 ID。
     * @return 当前 GameRoom 的房间 ID。
     */
    int64_t RoomId() const;

    /**
     * @brief 初始化本局战斗玩家和初始状态。
     * @param player_ids 参与本局的玩家 ID 列表。
     */
    void Start(const std::vector<int64_t>& player_ids);

    /**
     * @brief 停止本局战斗。
     */
    void Stop();

    /**
     * @brief 写入玩家输入。
     * @param input 玩家输入指令。
     */
    void HandleInput(const InputCommand& input);

    /**
     * @brief 推进一帧游戏逻辑。
     * @help 会消费 InputBuffer 中的输入，并更新玩家位置、血量和存活状态。
     */
    void Tick();

    /**
     * @brief 判断本局是否结束。
     * @return 已结束返回 true，否则返回 false。
     */
    bool IsGameOver() const;

    /**
     * @brief 获取胜者 ID。
     * @return 胜者玩家 ID，未结束时通常为 0。
     */
    int64_t WinnerId() const;

    /**
     * @brief 获取当前游戏状态快照。
     * @return 当前 GameState 的拷贝。
     */
    GameState State() const;

private:
    /**
     * @brief 应用一条玩家输入到当前状态。
     * @param input 玩家输入指令。
     */
    void ApplyInput(const InputCommand& input);

    /**
     * @brief 检查是否只剩一个存活玩家。
     */
    void CheckGameOver();

    int64_t room_id_;
    mutable std::mutex mutex_;
    GameState state_;
    InputBuffer input_buffer_;
    bool game_over_ = false;
    int64_t winner_id_ = 0;
};

} // namespace game
