#pragma once

#include "net/session.h"

#include <string>
#include <mutex>
#include <unordered_map>

namespace game {

class MessageDispatcher;
class ServiceContext;

class LoginService {
public:
    explicit LoginService(ServiceContext* context);

    void RegisterHandlers(MessageDispatcher& dispatcher);

    void HandleLogin(const SessionPtr& session, const Packet& packet);
    void HandleHeartbeat(const SessionPtr& session, const Packet& packet);
    void HandleReconnect(const SessionPtr& session, const Packet& packet);

private:
    std::string GenerateSessionToken(int64_t player_id);
    int64_t GetOrCreatePlayerId(const std::string& account);

    ServiceContext* context_;
    std::mutex mutex_;
    std::unordered_map<std::string, int64_t> account_players_;
    std::unordered_map<int64_t, std::string> session_tokens_;
};

} // namespace game
