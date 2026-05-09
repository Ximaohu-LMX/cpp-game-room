#include "match/match_queue.h"

#include <algorithm>

namespace game {

bool MatchQueue::Push(int64_t player_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (player_id == 0 || in_queue_.count(player_id) != 0) {
        return false;
    }
    queue_.push_back(player_id);
    in_queue_.insert(player_id);
    return true;
}

bool MatchQueue::Remove(int64_t player_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (in_queue_.erase(player_id) == 0) {
        return false;
    }
    queue_.erase(std::remove(queue_.begin(), queue_.end(), player_id), queue_.end());
    return true;
}

bool MatchQueue::Contains(int64_t player_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return in_queue_.count(player_id) != 0;
}

std::vector<int64_t> MatchQueue::PopN(size_t n) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<int64_t> result;
    if (queue_.size() < n) {
        return result;
    }
    result.reserve(n);
    for (size_t i = 0; i < n; ++i) {
        auto player_id = queue_.front();
        queue_.pop_front();
        in_queue_.erase(player_id);
        result.push_back(player_id);
    }
    return result;
}

size_t MatchQueue::Size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.size();
}

} // namespace game

