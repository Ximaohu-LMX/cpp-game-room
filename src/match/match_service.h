#pragma once

#include "match/match_queue.h"
#include "net/session.h"

namespace game {

class MessageDispatcher;
class ServiceContext;

/**
 * @brief 匹配服务。
 * @help 负责处理玩家加入/取消匹配，并在人数足够时创建房间。
 */
class MatchService {
public:
    /**
     * @brief 创建匹配服务。
     * @param context 服务依赖上下文。
     */
    explicit MatchService(ServiceContext* context);

    /**
     * @brief 注册匹配相关消息处理函数。
     * @param dispatcher 消息分发器。
     */
    void RegisterHandlers(MessageDispatcher& dispatcher);

    /**
     * @brief 处理加入匹配请求。
     * @param session 当前会话。
     * @param packet 匹配请求包。
     */
    void HandleMatch(const SessionPtr& session, const Packet& packet);

    /**
     * @brief 处理取消匹配请求。
     * @param session 当前会话。
     * @param packet 取消匹配请求包。
     */
    void HandleCancelMatch(const SessionPtr& session, const Packet& packet);

private:
    /**
     * @brief 尝试从匹配队列中取人并创建房间。
     */
    void TryCreateRoom();

    ServiceContext* context_;
    MatchQueue queue_;
};

} // namespace game
