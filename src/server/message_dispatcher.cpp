#include "server/message_dispatcher.h"

#include "util/logger.h"

namespace game {

void MessageDispatcher::RegisterHandler(uint32_t msg_id, Handler handler) {
    std::lock_guard<std::mutex> lock(mutex_);
    handlers_[msg_id] = std::move(handler);
}

void MessageDispatcher::Dispatch(const SessionPtr& session, const Packet& packet) {
    Handler handler;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = handlers_.find(packet.msg_id);
        if (it == handlers_.end()) {
            LOG_WARN("unknown msg_id {}", packet.msg_id);
            return;
        }
        handler = it->second;
    }
    handler(session, packet);
}

} // namespace game

