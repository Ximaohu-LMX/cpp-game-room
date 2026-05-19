#pragma once

#include <chrono>
#include <cstdint>

namespace game {

/**
 * @brief 获取当前系统时间毫秒数。
 * @return Unix epoch 毫秒时间戳。
 */
inline int64_t NowMs() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

} // namespace game
