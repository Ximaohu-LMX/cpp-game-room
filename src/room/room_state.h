#pragma once

namespace game {

/**
 * @brief 房间生命周期状态。
 */
enum class RoomState {
    Waiting = 0,    ///< 等待玩家准备。
    Ready = 1,      ///< 所有玩家已准备。
    Playing = 2,    ///< 游戏进行中。
    Settlement = 3, ///< 结算中。
    Closed = 4      ///< 房间已关闭。
};

} // namespace game
