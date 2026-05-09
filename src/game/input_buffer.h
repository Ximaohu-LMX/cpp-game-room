#pragma once

#include <cstdint>
#include <mutex>
#include <vector>

namespace game {

struct InputCommand {
    int64_t player_id = 0;
    int64_t input_seq = 0;
    float move_x = 0;
    float move_y = 0;
    bool fire = false;
    int64_t timestamp_ms = 0;
};

class InputBuffer {
public:
    void Push(const InputCommand& input);
    std::vector<InputCommand> PopAll();

private:
    std::mutex mutex_;
    std::vector<InputCommand> inputs_;
};

} // namespace game

