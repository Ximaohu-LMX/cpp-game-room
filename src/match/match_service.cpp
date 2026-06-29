#include "match/match_service.h"

#include "config/config_manager.h"
#include "player/player_manager.h"
#include "protocol/message_id.h"
#include "protocol/proto_helper.h"
#include "room/room_manager.h"
#include "server/message_dispatcher.h"
#include "server/service_context.h"
#include "util/logger.h"

#include "match.pb.h"

namespace game {

MatchService::MatchService(ServiceContext* context) : context_(context) {}

void MatchService::RegisterHandlers(MessageDispatcher& dispatcher) {
    dispatcher.RegisterHandler(MSG_MATCH_REQ, [this](const SessionPtr& session, const Packet& packet) {
        HandleMatch(session, packet);
    });
    dispatcher.RegisterHandler(MSG_MATCH_CANCEL_REQ, [this](const SessionPtr& session, const Packet& packet) {
        HandleCancelMatch(session, packet);
    });
}

void MatchService::HandleMatch(const SessionPtr& session, const Packet& packet) {
    proto::MatchRequest request;
    if (!ProtoHelper::Parse(packet, &request) || !session) {
        return;
    }

    const int64_t player_id = request.player_id() == 0 ? session->PlayerId() : request.player_id();
    proto::MatchResponse response;
    if (player_id == 0) {
        response.set_code(1);
        response.set_message("not login");
        session->Send(MSG_MATCH_RESP, response);
        return;
    }

    StartMatching(player_id, session, "matching");
}

void MatchService::HandleCancelMatch(const SessionPtr& session, const Packet& packet) {
    proto::MatchCancelRequest request;
    if (!ProtoHelper::Parse(packet, &request) || !session) {
        return;
    }
    const int64_t player_id = request.player_id() == 0 ? session->PlayerId() : request.player_id();
    proto::MatchCancelResponse response;
    if (queue_.Remove(player_id)) {
        response.set_code(0);
        response.set_message("cancelled");
    } else {
        response.set_code(2);
        response.set_message("not matching");
    }

    if (response.code() == 0 && context_ && context_->player_manager) {
        auto player = context_->player_manager->GetOrCreatePlayer(player_id);
        player->SetStatus(PlayerStatus::Online);
        player->SetRoomId(0);
        session->SetRoomId(0);
    }
    session->Send(MSG_MATCH_CANCEL_RESP, response);
}

bool MatchService::RequeuePlayer(int64_t player_id) {
    if (!context_ || !context_->connection_manager) {
        return false;
    }
    auto session = context_->connection_manager->GetByPlayerId(player_id);
    if (!session) {
        return false;
    }
    return StartMatching(player_id, session, "matching");
}

bool MatchService::StartMatching(int64_t player_id, const SessionPtr& session, const std::string& message) {
    proto::MatchResponse response;
    if (player_id == 0 || !session) {
        if (session) {
            response.set_code(1);
            response.set_message("not login");
            session->Send(MSG_MATCH_RESP, response);
        }
        return false;
    }

    if (!queue_.Push(player_id)) {
        if (queue_.Contains(player_id) && context_ && context_->player_manager) {
            auto player = context_->player_manager->GetOrCreatePlayer(player_id);
            player->SetStatus(PlayerStatus::Matching);
            player->SetRoomId(0);
            session->SetRoomId(0);
        }
        response.set_code(2);
        response.set_message("already matching");
        session->Send(MSG_MATCH_RESP, response);
        return false;
    }

    if (context_ && context_->player_manager) {
        auto player = context_->player_manager->GetOrCreatePlayer(player_id);
        player->SetStatus(PlayerStatus::Matching);
        player->SetRoomId(0);
    }
    session->SetRoomId(0);

    response.set_code(0);
    response.set_message(message);
    session->Send(MSG_MATCH_RESP, response);
    TryCreateRoom();    return true;
}

void MatchService::TryCreateRoom() {
    const auto need_count = static_cast<size_t>(ConfigManager::Instance().RoomPlayerCount());
    while (true) {
        auto players = queue_.TryPopN(need_count);
        if (players.empty()) {
            return;
        }
        if (players.size() != need_count || !context_ || !context_->room_manager) {
            return;
        }

        auto room = context_->room_manager->CreateRoom(players);
        proto::MatchSuccessNotify notify;
        notify.set_room_id(room->RoomId());
        for (auto player_id : players) {
            notify.add_player_ids(player_id);
        }

        for (auto player_id : players) {
            if (context_->player_manager) {
                auto player = context_->player_manager->GetOrCreatePlayer(player_id);
                player->SetStatus(PlayerStatus::InRoom);
                player->SetRoomId(room->RoomId());
            }
            if (context_->connection_manager) {
                if (auto session = context_->connection_manager->GetByPlayerId(player_id)) {
                    session->Send(MSG_MATCH_SUCCESS_NOTIFY, notify);
                }
            }
        }
        LOG_INFO("match success room {}", room->RoomId());
    }
}

} // namespace game
