#pragma once

#include <random>

namespace game {

/**
 * @brief 随机数工具。
 * @help 使用 thread_local 随机引擎，当前主要供 bot 生成随机输入。
 */
class RandomUtil {
public:
    /**
     * @brief 生成闭区间整数随机数。
     * @param min_value 最小值。
     * @param max_value 最大值。
     * @return 随机整数。
     */
    static int Int(int min_value, int max_value) {
        thread_local std::mt19937 rng{std::random_device{}()};
        std::uniform_int_distribution<int> dist(min_value, max_value);
        return dist(rng);
    }

    /**
     * @brief 生成浮点随机数。
     * @param min_value 最小值。
     * @param max_value 最大值。
     * @return 随机浮点数。
     */
    static float Float(float min_value, float max_value) {
        thread_local std::mt19937 rng{std::random_device{}()};
        std::uniform_real_distribution<float> dist(min_value, max_value);
        return dist(rng);
    }
};

} // namespace game
