#pragma once

#include <cstdint>
#include <cstddef>
#include <deque>
#include <mutex>
#include <unordered_set>
#include <vector>

namespace game {

class MatchQueue {
public:
    bool Push(int64_t player_id);
    bool Remove(int64_t player_id);
    bool Contains(int64_t player_id) const;

    std::vector<int64_t> PopN(size_t n);
    size_t Size() const;

private:
    mutable std::mutex mutex_;
    std::deque<int64_t> queue_;
    std::unordered_set<int64_t> in_queue_;
};

} // namespace game
