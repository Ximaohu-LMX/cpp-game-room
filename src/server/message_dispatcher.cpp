#include "server/message_dispatcher.h"

#include "util/logger.h"
#include "util/metrics.h"
#include "protocol/message_id.h"

namespace game {

void MessageDispatcher::RegisterHandler(uint32_t msg_id, Handler handler) {
    std::lock_guard<std::mutex> lock(mutex_);
    handlers_[msg_id] = std::move(handler);
}

void MessageDispatcher::Dispatch(const SessionPtr& session, const Packet& packet) {
    ScopedMetric elapsed(Distribution::MessageHandler);
    ScopedMetric login_elapsed(packet.msg_id == MSG_LOGIN_REQ ? Distribution::LoginHandler : Distribution::Count);
    Metrics::Instance().Increment(Counter::MessagesReceived);
    Handler handler;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = handlers_.find(packet.msg_id);
        if (it == handlers_.end()) {
            Metrics::Instance().Increment(Counter::UnknownMessages);
            LOG_WARN("unknown msg_id {}", packet.msg_id);
            return;
        }
        handler = it->second;
    }
    handler(session, packet);
}

} // namespace game

