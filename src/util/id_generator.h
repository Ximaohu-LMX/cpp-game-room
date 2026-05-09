#pragma once

#include <atomic>
#include <cstdint>

namespace game {

class IdGenerator {
public:
    static int64_t NextPlayerId() {
        static std::atomic<int64_t> id{100000};
        return id.fetch_add(1);
    }

    static int64_t NextRoomId() {
        static std::atomic<int64_t> id{1000};
        return id.fetch_add(1);
    }

    static int64_t NextBattleId() {
        static std::atomic<int64_t> id{1000000};
        return id.fetch_add(1);
    }

    static int64_t NextSessionId() {
        static std::atomic<int64_t> id{1};
        return id.fetch_add(1);
    }
};

} // namespace game

