#include "game/input_buffer.h"

namespace game {

void InputBuffer::Push(const InputCommand& input) {
    std::lock_guard<std::mutex> lock(mutex_);
    inputs_.push_back(input);
}

std::vector<InputCommand> InputBuffer::PopAll() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<InputCommand> result;
    result.swap(inputs_);
    return result;
}

} // namespace game

