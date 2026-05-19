#pragma once

#include <cstdint>
#include <mutex>
#include <vector>

namespace game {

/**
 * @brief 玩家输入指令。
 * @help 网络线程收到 InputRequest 后转换为 InputCommand，GameLoop 每帧统一消费。
 */
struct InputCommand {
    int64_t player_id = 0;    ///< 发出输入的玩家 ID。
    int64_t input_seq = 0;    ///< 客户端输入序号。
    float move_x = 0;         ///< x 方向移动输入，通常在 [-1, 1]。
    float move_y = 0;         ///< y 方向移动输入，通常在 [-1, 1]。
    bool fire = false;        ///< 是否开火。
    int64_t timestamp_ms = 0; ///< 服务端收到输入的时间戳。
};

/**
 * @brief 线程安全的输入缓冲区。
 * @help 网络线程生产输入，GameLoop 消费输入，是简化的生产者-消费者结构。
 */
class InputBuffer {
public:
    /**
     * @brief 写入一条输入。
     * @param input 玩家输入指令。
     */
    void Push(const InputCommand& input);

    /**
     * @brief 取出并清空当前所有输入。
     * @return 本轮 tick 待消费的输入列表。
     */
    std::vector<InputCommand> PopAll();

private:
    std::mutex mutex_;
    std::vector<InputCommand> inputs_;
};

} // namespace game
