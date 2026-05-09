#include "bot_manager.h"

#include <chrono>
#include <cstdint>
#include <iostream>
#include <string>
#include <thread>

int main(int argc, char** argv) {
    std::string host = "127.0.0.1";
    uint16_t port = 9000;
    int count = 100;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--host" && i + 1 < argc) {
            host = argv[++i];
        } else if (arg == "--port" && i + 1 < argc) {
            port = static_cast<uint16_t>(std::stoi(argv[++i]));
        } else if (arg == "--count" && i + 1 < argc) {
            count = std::stoi(argv[++i]);
        }
    }

    game::BotManager manager(host, port);
    manager.Start(count);
    std::cout << "started " << count << " bots, press Ctrl+C to stop\n";
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    return 0;
}

