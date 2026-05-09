#pragma once

#include "match/match_queue.h"
#include "net/session.h"

namespace game {

class MessageDispatcher;
class ServiceContext;

class MatchService {
public:
    explicit MatchService(ServiceContext* context);

    void RegisterHandlers(MessageDispatcher& dispatcher);

    void HandleMatch(const SessionPtr& session, const Packet& packet);
    void HandleCancelMatch(const SessionPtr& session, const Packet& packet);

private:
    void TryCreateRoom();

    ServiceContext* context_;
    MatchQueue queue_;
};

} // namespace game

