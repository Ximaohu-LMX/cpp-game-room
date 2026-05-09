#include "login/login_service.h"

#include "player/player_manager.h"
#include "protocol/message_id.h"
#include "protocol/proto_helper.h"
#include "room/room_manager.h"
#include "server/message_dispatcher.h"
#include "server/service_context.h"
#include "storage/player_repository.h"
#include "util/id_generator.h"
#include "util/logger.h"
#include "util/time_util.h"

#include "login.pb.h"

namespace game {

LoginService::LoginService(ServiceContext* context) : context_(context) {}

void LoginService::RegisterHandlers(MessageDispatcher& dispatcher) {
    dispatcher.RegisterHandler(MSG_LOGIN_REQ, [this](const SessionPtr& session, const Packet& packet) {
        HandleLogin(session, packet);
    });
    dispatcher.RegisterHandler(MSG_HEARTBEAT_REQ, [this](const SessionPtr& session, const Packet& packet) {
        HandleHeartbeat(session, packet);
    });
    dispatcher.RegisterHandler(MSG_RECONNECT_REQ, [this](const SessionPtr& session, const Packet& packet) {
        HandleReconnect(session, packet);
    });
}

void LoginService::HandleLogin(const SessionPtr& session, const Packet& packet) {
    if (!session) {
        return;
    }

    proto::LoginRequest request;
    proto::LoginResponse response;
    if (!ProtoHelper::Parse(packet, &request)) {
        response.set_code(1);
        session->Send(MSG_LOGIN_RESP, response);
        return;
    }

    const int64_t player_id = GetOrCreatePlayerId(request.account());
    auto token = GenerateSessionToken(player_id);
    {
        std::lock_guard<std::mutex> lock(mutex_);
        session_tokens_[player_id] = token;
    }

    if (context_ && context_->connection_manager) {
        if (auto old_session = context_->connection_manager->GetByPlayerId(player_id)) {
            if (old_session->SessionId() != session->SessionId()) {
                old_session->Close();
            }
        }
    }

    session->BindPlayerId(player_id);
    session->UpdateHeartbeatTime();
    if (context_ && context_->connection_manager) {
        context_->connection_manager->BindPlayer(player_id, session);
    }

    auto data = context_ && context_->player_repository
                    ? context_->player_repository->LoadPlayer(player_id)
                    : PlayerData{player_id, "player_" + std::to_string(player_id), 1000};

    if (context_ && context_->player_manager) {
        auto player = context_->player_manager->GetOrCreatePlayer(player_id, data.name);
        player->SetScore(data.score);
        context_->player_manager->SetOnline(player_id, session);
    }

    response.set_code(0);
    response.set_player_id(player_id);
    response.set_name(data.name);
    response.set_session_token(token);
    session->Send(MSG_LOGIN_RESP, response);
    LOG_INFO("player {} login", player_id);
}

void LoginService::HandleHeartbeat(const SessionPtr& session, const Packet&) {
    if (!session) {
        return;
    }
    session->UpdateHeartbeatTime();
    proto::HeartbeatResponse response;
    response.set_server_time_ms(NowMs());
    session->Send(MSG_HEARTBEAT_RESP, response);
}

void LoginService::HandleReconnect(const SessionPtr& session, const Packet& packet) {
    if (!session) {
        return;
    }
    proto::ReconnectRequest request;
    proto::ReconnectResponse response;
    if (!ProtoHelper::Parse(packet, &request)) {
        response.set_code(1);
        session->Send(MSG_RECONNECT_RESP, response);
        return;
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto token_it = session_tokens_.find(request.player_id());
        if (token_it == session_tokens_.end() || token_it->second != request.session_token()) {
            response.set_code(2);
            session->Send(MSG_RECONNECT_RESP, response);
            return;
        }
    }

    session->BindPlayerId(request.player_id());
    if (context_ && context_->connection_manager) {
        context_->connection_manager->BindPlayer(request.player_id(), session);
    }
    if (context_ && context_->player_manager) {
        context_->player_manager->SetOnline(request.player_id(), session);
    }
    if (context_ && context_->room_manager) {
        auto room = context_->room_manager->GetPlayerRoom(request.player_id());
        if (room) {
            room->OnPlayerReconnect(request.player_id(), session);
            response.set_room_id(room->RoomId());
        }
    }
    response.set_code(0);
    session->Send(MSG_RECONNECT_RESP, response);
}

std::string LoginService::GenerateSessionToken(int64_t player_id) {
    return std::to_string(player_id) + "_" + std::to_string(NowMs());
}

int64_t LoginService::GetOrCreatePlayerId(const std::string& account) {
    auto key = account.empty() ? "guest_" + std::to_string(IdGenerator::NextPlayerId()) : account;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = account_players_.find(key);
        if (it != account_players_.end()) {
            return it->second;
        }
    }

    const int64_t player_id = IdGenerator::NextPlayerId();
    {
        std::lock_guard<std::mutex> lock(mutex_);
        account_players_[key] = player_id;
    }
    if (context_ && context_->player_repository) {
        PlayerData data;
        data.player_id = player_id;
        data.name = "player_" + std::to_string(player_id);
        context_->player_repository->CreatePlayer(data);
    }
    return player_id;
}

} // namespace game
