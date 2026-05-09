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

    if (!queue_.Push(player_id)) {
        response.set_code(2);
        response.set_message("already matching");
        session->Send(MSG_MATCH_RESP, response);
        return;
    }

    if (context_ && context_->player_manager) {
        auto player = context_->player_manager->GetOrCreatePlayer(player_id);
        player->SetStatus(PlayerStatus::Matching);
    }

    response.set_code(0);
    response.set_message("matching");
    session->Send(MSG_MATCH_RESP, response);
    TryCreateRoom();
}

void MatchService::HandleCancelMatch(const SessionPtr& session, const Packet& packet) {
    proto::MatchCancelRequest request;
    if (!ProtoHelper::Parse(packet, &request) || !session) {
        return;
    }
    const int64_t player_id = request.player_id() == 0 ? session->PlayerId() : request.player_id();
    queue_.Remove(player_id);
}

void MatchService::TryCreateRoom() {
    const auto need_count = static_cast<size_t>(ConfigManager::Instance().RoomPlayerCount());
    while (queue_.Size() >= need_count) {
        auto players = queue_.PopN(need_count);
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
