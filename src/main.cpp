#include "config/config_manager.h"
#include "server/game_server.h"
#include "util/logger.h"

#include <iostream>
#include <cstdlib>
#include "util/metrics.h"

int main() {
    game::Logger::Init();
    const char* config_path = std::getenv("GAME_SERVER_CONFIG");
    game::ConfigManager::Instance().Load(config_path && *config_path ? config_path : "config/server.yaml");

    game::GameServer server;
    if (!server.Init()) {
        std::cerr << "failed to init game server\n";
        return 1;
    }

    game::MetricsReporter metrics;
    const char* metrics_path = std::getenv("GAME_METRICS_FILE");
    if (metrics_path && *metrics_path &&
        !metrics.Start(metrics_path, [&server]() { return server.MeasurementGauges(); })) {
        std::cerr << "failed to open metrics output\n";
        return 1;
    }
    server.Start();
    return 0;
}

