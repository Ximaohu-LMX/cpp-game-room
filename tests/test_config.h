#pragma once

#include "config/config_manager.h"

#include <cstdlib>
#include <string>

// 使用临时数据库运行集成测试时，可覆盖默认配置而不修改 config/server.yaml。
inline bool LoadIntegrationTestConfig() {
    const char* path = std::getenv("GAME_TEST_CONFIG");
    return game::ConfigManager::Instance().Load(
        path && *path ? path : std::string(GAME_TEST_SOURCE_DIR) + "/config/server.yaml");
}
