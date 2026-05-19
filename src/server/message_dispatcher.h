#pragma once

#include "net/packet.h"
#include "net/session.h"

#include <functional>
#include <mutex>
#include <unordered_map>

namespace game {

/**
 * @brief 消息处理函数类型。
 * @param session 当前连接会话。
 * @param packet 已解码的网络包。
 */
using Handler = std::function<void(const SessionPtr&, const Packet&)>;

/**
 * @brief 按 msg_id 分发消息的调度器。
 * @help 网络层只负责收包，业务服务通过 RegisterHandler 注册自己的消息处理函数。
 */
class MessageDispatcher {
public:
    /**
     * @brief 注册消息处理函数。
     * @param msg_id 消息 ID。
     * @param handler 对应业务处理函数。
     */
    void RegisterHandler(uint32_t msg_id, Handler handler);

    /**
     * @brief 分发消息。
     * @param session 当前连接会话。
     * @param packet 已解码的网络包。
     */
    void Dispatch(const SessionPtr& session, const Packet& packet);

private:
    std::mutex mutex_;
    std::unordered_map<uint32_t, Handler> handlers_;
};

} // namespace game
