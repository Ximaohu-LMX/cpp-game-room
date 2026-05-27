#include "bot_manager.h"

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <iostream>
#include <string>
#include <thread>

namespace {

std::atomic<bool> g_running{true};

void OnSignal(int) {
    g_running = false;
}

void PrintUsage(const char* program) {
    std::cout
        << "Usage: " << program << " [options]\n"
        << "  --host HOST                         default: 127.0.0.1\n"
        << "  --port PORT                         default: 9000\n"
        << "  --count N                           default: 100\n"
        << "  --duration SEC                      default: 0, run until Ctrl+C\n"
        << "  --cancel-match-percent N            default: 10\n"
        << "  --queue-disconnect-percent N        default: 5\n"
        << "  --room-disconnect-percent N         default: 5\n"
        << "  --playing-disconnect-percent N      default: 5\n"
        << "  --ready-toggle-percent N            default: 10\n"
        << "  --max-reconnects N                  default: 2\n"
        << "  --reconnect-delay-ms N              default: 500\n"
        << "  --rematch-delay-ms N                default: 500\n"
        << "  --action-jitter-ms N                default: 800\n"
        << "  --heartbeat-interval-ms N           default: 5000, <=0 disables heartbeat\n"
        << "  --input-interval-ms N               default: 50\n"
        << "  --verbose                           print per-bot disconnect traces\n";
}

} // namespace

int main(int argc, char** argv) {
    std::string host = "127.0.0.1";
    uint16_t port = 9000;
    int count = 100;
    int duration_seconds = 0;
    game::BotOptions options;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help") {
            PrintUsage(argv[0]);
            return 0;
        } else if (arg == "--host" && i + 1 < argc) {
            host = argv[++i];
        } else if (arg == "--port" && i + 1 < argc) {
            port = static_cast<uint16_t>(std::stoi(argv[++i]));
        } else if (arg == "--count" && i + 1 < argc) {
            count = std::stoi(argv[++i]);
        } else if (arg == "--duration" && i + 1 < argc) {
            duration_seconds = std::stoi(argv[++i]);
        } else if (arg == "--cancel-match-percent" && i + 1 < argc) {
            options.cancel_match_percent = std::stoi(argv[++i]);
        } else if (arg == "--queue-disconnect-percent" && i + 1 < argc) {
            options.queue_disconnect_percent = std::stoi(argv[++i]);
        } else if (arg == "--room-disconnect-percent" && i + 1 < argc) {
            options.room_disconnect_percent = std::stoi(argv[++i]);
        } else if (arg == "--playing-disconnect-percent" && i + 1 < argc) {
            options.playing_disconnect_percent = std::stoi(argv[++i]);
        } else if (arg == "--ready-toggle-percent" && i + 1 < argc) {
            options.ready_toggle_percent = std::stoi(argv[++i]);
        } else if (arg == "--max-reconnects" && i + 1 < argc) {
            options.max_reconnects = std::stoi(argv[++i]);
        } else if (arg == "--reconnect-delay-ms" && i + 1 < argc) {
            options.reconnect_delay_ms = std::stoi(argv[++i]);
        } else if (arg == "--rematch-delay-ms" && i + 1 < argc) {
            options.rematch_delay_ms = std::stoi(argv[++i]);
        } else if (arg == "--action-jitter-ms" && i + 1 < argc) {
            options.action_jitter_ms = std::stoi(argv[++i]);
        } else if (arg == "--heartbeat-interval-ms" && i + 1 < argc) {
            options.heartbeat_interval_ms = std::stoi(argv[++i]);
        } else if (arg == "--input-interval-ms" && i + 1 < argc) {
            options.input_interval_ms = std::stoi(argv[++i]);
        } else if (arg == "--verbose") {
            options.verbose = true;
        } else {
            std::cerr << "unknown or incomplete option: " << arg << "\n";
            PrintUsage(argv[0]);
            return 1;
        }
    }

    std::signal(SIGINT, OnSignal);
    std::signal(SIGTERM, OnSignal);

    game::BotManager manager(host, port, options);
    manager.Start(count);
    std::cout << "started " << count << " bots on " << host << ":" << port << "\n";

    int64_t elapsed = 0;
    while (g_running && (duration_seconds <= 0 || elapsed < duration_seconds)) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        ++elapsed;
        manager.PrintStats(elapsed);
    }

    manager.Stop();
    manager.PrintStats(elapsed);
    return 0;
}
