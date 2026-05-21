#pragma once

#include "storage/mysql_client.h"

#include <cstdint>
#include <string>

namespace game {

/**
 * @brief 一局战斗的总记录。
 */
struct Battle {
    int64_t battle_id = 0; ///< 战斗 ID。
    int64_t room_id = 0; ///< 房间 ID。
    int64_t winner_id = 0; ///< 本局唯一胜者 ID。
    std::string result_json; ///< 战斗结果扩展字段。
};

/**
 * @brief 单个玩家在一局战斗中的结果。
 */
struct BattlePlayerResult {
    int64_t battle_id = 0; ///< 战斗 ID。
    int64_t player_id = 0; ///< 玩家 ID。
    std::string result; ///< WIN 或 LOSE。
    int score_delta = 0; ///< 本局积分变化。
};

/**
 * @brief 结算日志，用于幂等控制。
 */
struct SettlementLog {
    int64_t settlement_id = 0; ///< 结算日志 ID。
    int64_t battle_id = 0; ///< 战斗 ID。
    int64_t player_id = 0; ///< 玩家 ID。
    int score_delta = 0; ///< 积分变化。
};

/**
 * @brief 战斗仓储。
 * @help 封装 battle、battle_player_result 和 settlement_log 的读写。
 */
class BattleRepository {
public:
    /**
     * @brief 创建战斗仓储。
     * @param mysql MySQL 客户端。
     */
    explicit BattleRepository(MysqlClient* mysql);

    /**
     * @brief 插入战斗总记录。
     * @param battle 战斗记录。
     * @return 插入成功返回 true；重复或失败返回 false。
     */
    bool InsertBattle(const Battle& battle);

    /**
     * @brief 插入玩家战斗结果。
     * @param result 玩家结果记录。
     * @return 插入成功返回 true；重复或失败返回 false。
     */
    bool InsertBattlePlayerResult(const BattlePlayerResult& result);

    /**
     * @brief 插入结算日志。
     * @param log 结算日志。
     * @return 插入成功返回 true；重复结算返回 false。
     */
    bool InsertSettlementLog(const SettlementLog& log);

    /**
     * @brief 判断某玩家在某场战斗中是否已结算。
     * @param battle_id 战斗 ID。
     * @param player_id 玩家 ID。
     * @return 已结算返回 true，否则返回 false。
     */
    bool HasSettlement(int64_t battle_id, int64_t player_id);

private:
    MysqlClient* mysql_;
};

} // namespace game
