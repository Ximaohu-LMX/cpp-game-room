#pragma once

#include <cstdint>
#include <memory>
#include <string>

namespace game {

/**
 * @brief 玩家运行时状态。
 */
enum class PlayerStatus {
    Offline,  ///< 离线。
    Online,   ///< 在线但未匹配。
    Matching, ///< 匹配中。
    InRoom,   ///< 已进入房间。
    Playing   ///< 战斗中。
};

/**
 * @brief 玩家内存态对象。
 * @help 保存玩家 ID、昵称、积分、当前状态和房间 ID。
 */
class Player {
public:
    /**
     * @brief 创建玩家对象。
     * @param player_id 玩家 ID。
     * @param name 玩家名。
     * @param score 初始积分。
     */
    Player(int64_t player_id, std::string name, int score = 1000);

    /** @brief 获取玩家 ID。 @return 玩家 ID。 */
    int64_t Id() const;
    /** @brief 获取玩家名。 @return 玩家名引用。 */
    const std::string& Name() const;

    /** @brief 设置玩家状态。 @param status 新状态。 */
    void SetStatus(PlayerStatus status);
    /** @brief 获取玩家状态。 @return 当前状态。 */
    PlayerStatus Status() const;

    /** @brief 设置积分。 @param score 新积分。 */
    void SetScore(int score);
    /** @brief 获取积分。 @return 当前积分。 */
    int Score() const;

    /** @brief 设置当前房间 ID。 @param room_id 房间 ID。 */
    void SetRoomId(int64_t room_id);
    /** @brief 获取当前房间 ID。 @return 房间 ID，未进房为 0。 */
    int64_t RoomId() const;

private:
    int64_t player_id_;
    std::string name_;
    int score_;
    PlayerStatus status_ = PlayerStatus::Offline;
    int64_t room_id_ = 0;
};

/** @brief Player 共享指针类型。 */
using PlayerPtr = std::shared_ptr<Player>;

} // namespace game
