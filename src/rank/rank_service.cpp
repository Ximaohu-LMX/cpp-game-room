#include "rank/rank_service.h"

#include "protocol/message_id.h"
#include "protocol/proto_helper.h"
#include "server/message_dispatcher.h"

#include "rank.pb.h"

#include <cstdlib>
#include <string>

namespace game {

namespace {
constexpr const char* kRankScoreKey = "rank:score";
}

RankService::RankService(RedisClient* redis) : redis_(redis) {}

bool RankService::Init() {
    return redis_ == nullptr || redis_->Connect();
}

void RankService::RegisterHandlers(MessageDispatcher& dispatcher) {
    dispatcher.RegisterHandler(MSG_RANK_REQ, [this](const SessionPtr& session, const Packet& packet) {
        HandleRankRequest(session, packet);
    });
}

void RankService::UpdateScore(int64_t player_id, int score) {
    if (!redis_) {
        return;
    }
    redis_->ZAdd(kRankScoreKey, score, std::to_string(player_id));
}

std::vector<RankEntry> RankService::GetTopN(int offset, int limit) {
    std::vector<RankEntry> entries;
    if (!redis_ || limit <= 0) {
        return entries;
    }

    if (offset < 0) {
        offset = 0;
    }
    const int stop = offset + limit - 1;
    const auto items = redis_->ZRevRange(kRankScoreKey, offset, stop);
    entries.reserve(items.size());
    for (size_t i = 0; i < items.size(); ++i) {
        RankEntry entry;
        entry.player_id = std::strtoll(items[i].first.c_str(), nullptr, 10);
        entry.name = "player_" + items[i].first;
        entry.score = items[i].second;
        entry.rank = offset + static_cast<int>(i) + 1;
        entries.push_back(std::move(entry));
    }
    return entries;
}

void RankService::HandleRankRequest(const SessionPtr& session, const Packet& packet) {
    proto::RankRequest request;
    if (!ProtoHelper::Parse(packet, &request)) {
        return;
    }

    proto::RankResponse response;
    for (const auto& item : GetTopN(request.offset(), request.limit())) {
        auto* rank_item = response.add_items();
        rank_item->set_player_id(item.player_id);
        rank_item->set_name(item.name);
        rank_item->set_score(item.score);
        rank_item->set_rank(item.rank);
    }
    session->Send(MSG_RANK_RESP, response);
}

} // namespace game
