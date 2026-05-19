#pragma once

#include <string>

#if defined(GAME_USE_SPDLOG) && GAME_USE_SPDLOG && __has_include(<spdlog/spdlog.h>)
#include <spdlog/spdlog.h>
#define GAME_HAS_SPDLOG 1
#endif

namespace game {

/**
 * @brief 日志初始化工具。
 * @help 如果开启 GAME_USE_SPDLOG，则初始化 spdlog；否则日志宏为空实现。
 */
class Logger {
public:
    /**
     * @brief 初始化日志系统。
     */
    static void Init();
};

} // namespace game

#if GAME_HAS_SPDLOG
#define LOG_INFO(...) spdlog::info(__VA_ARGS__)
#define LOG_ERROR(...) spdlog::error(__VA_ARGS__)
#define LOG_WARN(...) spdlog::warn(__VA_ARGS__)
#define LOG_DEBUG(...) spdlog::debug(__VA_ARGS__)
#else
#define LOG_INFO(...) do {} while (0)
#define LOG_ERROR(...) do {} while (0)
#define LOG_WARN(...) do {} while (0)
#define LOG_DEBUG(...) do {} while (0)
#endif
