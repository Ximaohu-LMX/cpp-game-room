#pragma once

#include "storage/battle_repository.h"
#include "storage/player_repository.h"

#include <cstdint>
#include <mutex>
#include <vector>

namespace game {

class RankService;

/**
 * @brief 战斗结算服务。
 * @help MySQL 事务提交战绩和积分；提交后同步 Redis，失败可复用同一 battle_id 重试。
 */
class SettlementService {
public:
    SettlementService(BattleRepository* battle_repository, PlayerRepository* player_repository, RankService* rank_service);

    /**
     * @brief 结算一局战斗，相同 ID 和相同结果可安全重试。
     * @param battle_id 由 BattleRepository::CreateBattle 分配，同一局必须一直复用。
     * @param room_id 房间 ID。
     * @param winner_id 胜者玩家 ID。
     * @param losers 失败玩家 ID 列表，允许重试时顺序不同。
     * @return MySQL 已提交且排行榜同步成功返回 true；失败时调用方保留原 ID 重试。
     */
    bool SettleBattle(int64_t battle_id, int64_t room_id, int64_t winner_id, const std::vector<int64_t>& losers);

private:
    BattleRepository* battle_repository_;
    PlayerRepository* player_repository_;
    RankService* rank_service_;
    // 本进程内串行化结算及随后的榜单刷新，防止旧积分覆盖新积分。
    std::mutex mutex_;
};

} // namespace game
