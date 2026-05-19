#pragma once

#include "storage/battle_repository.h"
#include "storage/player_repository.h"

#include <cstdint>
#include <vector>

namespace game {

class RankService;

/**
 * @brief 战斗结算服务。
 * @help 负责写入战斗结果、插入幂等结算日志、更新玩家积分和 Redis 排行榜。
 */
class SettlementService {
public:
    /**
     * @brief 创建结算服务。
     * @param battle_repository 战斗仓储。
     * @param player_repository 玩家仓储。
     * @param rank_service 排行榜服务。
     */
    SettlementService(BattleRepository* battle_repository, PlayerRepository* player_repository, RankService* rank_service);

    /**
     * @brief 结算一局战斗。
     * @param room_id 房间 ID。
     * @param winner_id 胜者玩家 ID。
     * @param losers 失败玩家 ID 列表。
     */
    void SettleBattle(int64_t room_id, int64_t winner_id, const std::vector<int64_t>& losers);

private:
    /**
     * @brief 插入结算日志。
     * @param battle_id 战斗 ID。
     * @param player_id 玩家 ID。
     * @param delta 积分变化。
     * @return 插入成功返回 true；重复结算或失败返回 false。
     */
    bool TryInsertSettlementLog(int64_t battle_id, int64_t player_id, int delta);

    /**
     * @brief 更新玩家积分并同步排行榜。
     * @param player_id 玩家 ID。
     * @param delta 积分变化。
     */
    void UpdatePlayerScore(int64_t player_id, int delta);

    BattleRepository* battle_repository_;
    PlayerRepository* player_repository_;
    RankService* rank_service_;
};

} // namespace game
