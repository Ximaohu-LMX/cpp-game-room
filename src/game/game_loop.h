#pragma once

#include "game/game_room.h"

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>

namespace game {

/**
 * @brief 固定 tick 游戏循环，负责定时推进所有运行中的 GameRoom。
 * @help GameLoop 不直接处理网络发送和结算，只在每个房间 tick 后通过 TickCallback 把状态交给外层。
 */
class GameLoop {
public:
    /**
     * @brief 单个房间 tick 后触发的回调。类型别名，以lambda方式传入。
     * @param room_id 房间 ID。
     * @param state 当前房间的游戏状态快照。
     * @param game_over 当前房间是否已经结束。
     * @param winner_id 胜者玩家 ID，未结束时通常为 0。
     */
    using TickCallback = std::function<void(int64_t, const GameState&, bool, int64_t)>;

    /**
     * @brief 构造固定 tick 循环。
     * @param tick_interval_ms tick 间隔，单位毫秒，非法值会回退到 50ms。
     */
    explicit GameLoop(int tick_interval_ms = 50);

    /**
     * @brief 析构时停止后台线程。
     */
    ~GameLoop();

    /**
     * @brief 启动 GameLoop 工作线程。
     */
    void Start();

    /**
     * @brief 停止 GameLoop 工作线程。
     */
    void Stop();

    /**
     * @brief 添加一个运行中的战斗房间。
     * @param room 待推进的 GameRoom。
     */
    void AddRoom(std::shared_ptr<GameRoom> room);

    /**
     * @brief 移除一个战斗房间。
     * @param room_id 要移除的房间 ID。
     */
    void RemoveRoom(int64_t room_id);

    /**
     * @brief 根据房间 ID 获取 GameRoom。
     * @param room_id 房间 ID。
     * @return 找到则返回 GameRoom，否则返回 nullptr。
     */
    std::shared_ptr<GameRoom> GetRoom(int64_t room_id);

    /**
     * @brief 设置每帧回调。
     * @param callback tick 后执行的回调函数。
     */
    void SetTickCallback(TickCallback callback);

private:
    /**
     * @brief GameLoop 主循环，按固定间隔推进所有房间。
     */
    void Loop();

    int tick_interval_ms_;
    std::mutex mutex_;
    std::unordered_map<int64_t, std::shared_ptr<GameRoom>> rooms_;  // 战斗房间表
    TickCallback tick_callback_;
    std::atomic<bool> running_{false};
    std::thread worker_;
};

} // namespace game
