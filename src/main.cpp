#include "config/config_manager.h"
#include "server/game_server.h"
#include "util/logger.h"

#include <iostream>

int main() {
    game::Logger::Init();
    game::ConfigManager::Instance().Load("config/server.yaml");

    game::GameServer server;
    if (!server.Init()) {
        std::cerr << "failed to init game server\n";
        return 1;
    }

    server.Start();
    return 0;
}

