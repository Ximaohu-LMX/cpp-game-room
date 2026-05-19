#pragma once

#include "net/session.h"

#include <string>
#include <mutex>
#include <unordered_map>

namespace game {

class MessageDispatcher;
class ServiceContext;

/**
 * @brief 登录服务。
 * @help 处理登录、心跳和重连，并完成 Session 与 player_id 的绑定。
 */
class LoginService {
public:
    /**
     * @brief 创建登录服务。
     * @param context 服务依赖上下文。
     */
    explicit LoginService(ServiceContext* context);

    /**
     * @brief 注册登录相关消息处理函数。
     * @param dispatcher 消息分发器。
     */
    void RegisterHandlers(MessageDispatcher& dispatcher);

    /**
     * @brief 处理登录请求。
     * @param session 当前会话。
     * @param packet 登录请求包。
     */
    void HandleLogin(const SessionPtr& session, const Packet& packet);

    /**
     * @brief 处理心跳请求。
     * @param session 当前会话。
     * @param packet 心跳请求包。
     */
    void HandleHeartbeat(const SessionPtr& session, const Packet& packet);

    /**
     * @brief 处理重连请求。
     * @param session 新连接会话。
     * @param packet 重连请求包。
     */
    void HandleReconnect(const SessionPtr& session, const Packet& packet);

private:
    /**
     * @brief 生成重连令牌。
     * @param player_id 玩家 ID。
     * @return session token 字符串。
     */
    std::string GenerateSessionToken(int64_t player_id);

    /**
     * @brief 根据账号获取或创建玩家 ID。
     * @param account 登录账号。
     * @return 玩家 ID。
     */
    int64_t GetOrCreatePlayerId(const std::string& account);

    ServiceContext* context_;
    std::mutex mutex_;
    std::unordered_map<std::string, int64_t> account_players_;
    std::unordered_map<int64_t, std::string> session_tokens_;
};

} // namespace game
