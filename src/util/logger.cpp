#include "util/logger.h"

#if GAME_HAS_SPDLOG
#include <spdlog/sinks/stdout_color_sinks.h>
#endif

namespace game {

void Logger::Init() {
#if GAME_HAS_SPDLOG
    auto logger = spdlog::stdout_color_mt("game");
    spdlog::set_default_logger(logger);
    spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");
    spdlog::set_level(spdlog::level::debug);
#endif
}

} // namespace game

