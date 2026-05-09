#pragma once

#include "net/packet.h"
#include "net/session.h"

#include <functional>
#include <mutex>
#include <unordered_map>

namespace game {

using Handler = std::function<void(const SessionPtr&, const Packet&)>;

class MessageDispatcher {
public:
    void RegisterHandler(uint32_t msg_id, Handler handler);
    void Dispatch(const SessionPtr& session, const Packet& packet);

private:
    std::mutex mutex_;
    std::unordered_map<uint32_t, Handler> handlers_;
};

} // namespace game

