#pragma once

#include <random>

namespace game {

class RandomUtil {
public:
    static int Int(int min_value, int max_value) {
        thread_local std::mt19937 rng{std::random_device{}()};
        std::uniform_int_distribution<int> dist(min_value, max_value);
        return dist(rng);
    }

    static float Float(float min_value, float max_value) {
        thread_local std::mt19937 rng{std::random_device{}()};
        std::uniform_real_distribution<float> dist(min_value, max_value);
        return dist(rng);
    }
};

} // namespace game

