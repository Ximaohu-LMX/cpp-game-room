#pragma once

#include "net/connection_manager.h"
#include "room/room_player.h"
#include "room/room_state.h"

#include <google/protobuf/message.h>

#include <cstdint>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace game {

/**
 * @brief 匹配成功后的房间对象。
 * @help 管理房间玩家、准备状态和房间状态机，并负责向房间内玩家广播消息。
 */
class Room {
public:
    /**
     * @brief 创建房间。
     * @param room_id 房间 ID。
     * @param connection_manager 连接管理器，用于广播时按 player_id 查找 Session。
     */
    Room(int64_t room_id, ConnectionManager* connection_manager);

    /**
     * @brief 添加玩家到房间。
     * @param player_id 玩家 ID。
     * @return 添加成功返回 true；房间状态不允许添加时返回 false。
     */
    bool AddPlayer(int64_t player_id);

    /**
     * @brief 从房间移除玩家。
     * @param player_id 玩家 ID。
     * @return 移除成功返回 true，否则返回 false。
     */
    bool RemovePlayer(int64_t player_id);

    /**
     * @brief 设置玩家准备状态。
     * @param player_id 玩家 ID。
     * @param ready 是否准备。
     */
    void SetReady(int64_t player_id, bool ready);

    /**
     * @brief 设置玩家准备状态，并在所有玩家准备后原子切换到 Playing。
     * @param player_id 玩家 ID。
     * @param ready 是否准备。
     * @return 本次调用是否把房间切换到了 Playing。
     */
    bool SetReadyAndTryStart(int64_t player_id, bool ready);

    /**
     * @brief 判断房间是否可以开始游戏。
     * @return 所有玩家准备完毕返回 true。
     */
    bool CanStart() const;

    /**
     * @brief 将房间切换到 Playing。
     */
    void StartGame();

    /**
     * @brief 将房间切换到 Settlement。
     */
    void SetSettlement();

    /**
     * @brief 关闭房间。
     */
    void Close();

    /**
     * @brief 标记玩家断线。
     * @param player_id 玩家 ID。
     */
    void OnPlayerDisconnect(int64_t player_id);

    /**
     * @brief 标记玩家重连并恢复 Session 房间 ID。
     * @param player_id 玩家 ID。
     * @param session 新会话。
     */
    void OnPlayerReconnect(int64_t player_id, SessionPtr session);

    /**
     * @brief 向房间内所有玩家广播 protobuf 消息。
     * @param msg_id 消息 ID。
     * @param msg protobuf 消息。
     */
    void Broadcast(uint32_t msg_id, const google::protobuf::Message& msg);

    /** @brief 获取房间状态。 @return 当前 RoomState。 */
    RoomState State() const;
    /** @brief 获取房间 ID。 @return room_id。 */
    int64_t RoomId() const;
    /** @brief 获取房间内玩家 ID 列表。 @return 玩家 ID 列表。 */
    std::vector<int64_t> PlayerIds() const;

private:
    int64_t room_id_;
    ConnectionManager* connection_manager_;
    mutable std::mutex mutex_;
    RoomState state_ = RoomState::Waiting;
    std::unordered_map<int64_t, RoomPlayer> players_;
};

/** @brief Room 共享指针类型。 */
using RoomPtr = std::shared_ptr<Room>;

} // namespace game
