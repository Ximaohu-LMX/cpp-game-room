#pragma once

#include "game/game_loop.h"
#include "room/room.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace game {

class MessageDispatcher;
class ServiceContext;

/**
 * @brief 房间管理器。
 * @help 负责创建/查找/移除房间，处理准备和输入消息，并把房间加入 GameLoop。
 */
class RoomManager {
public:
    /**
     * @brief 创建房间管理器。
     * @param context 服务依赖上下文。
     */
    explicit RoomManager(ServiceContext* context);

    /**
     * @brief 注册房间相关消息处理函数。
     * @param dispatcher 消息分发器。
     */
    void RegisterHandlers(MessageDispatcher& dispatcher);

    /**
     * @brief 创建房间。
     * @param player_ids 房间玩家 ID 列表。
     * @return 创建后的 Room。
     */
    RoomPtr CreateRoom(const std::vector<int64_t>& player_ids);

    /**
     * @brief 根据房间 ID 获取房间。
     * @param room_id 房间 ID。
     * @return 找到返回 Room，否则返回 nullptr。
     */
    RoomPtr GetRoom(int64_t room_id);

    /**
     * @brief 移除房间并清理玩家到房间的映射。
     * @param room_id 房间 ID。
     */
    void RemoveRoom(int64_t room_id);

    /**
     * @brief 根据玩家 ID 获取其所在房间。
     * @param player_id 玩家 ID。
     * @return 找到返回 Room，否则返回 nullptr。
     */
    RoomPtr GetPlayerRoom(int64_t player_id);

private:
    /**
     * @brief 处理玩家准备请求。
     * @param session 当前会话。
     * @param packet 准备请求包。
     */
    void HandleReady(const SessionPtr& session, const Packet& packet);

    /**
     * @brief 处理玩家输入请求。
     * @param session 当前会话。
     * @param packet 输入请求包。
     */
    void HandleInput(const SessionPtr& session, const Packet& packet);

    /**
     * @brief 创建 GameRoom 并加入 GameLoop。
     * @param room 业务房间。
     */
    void StartGameRoom(const RoomPtr& room);

    ServiceContext* context_;
    std::mutex mutex_;
    std::unordered_map<int64_t, RoomPtr> rooms_;
    std::unordered_map<int64_t, int64_t> player_room_map_;
};

} // namespace game
