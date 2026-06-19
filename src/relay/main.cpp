#include "vault/relay.hpp"
#include <iostream>
#include <fstream>
#include <csignal>
#include <atomic>
#include <cstring>

static std::atomic<bool> g_running{true};

void signalHandler(int) {
    g_running = false;
}

static RelayConfig loadConfig(const std::string& path) {
    RelayConfig cfg;
    std::ifstream f(path);
    if (!f.is_open()) {
        std::cerr << "Warning: could not open " << path << ", using defaults\n";
        return cfg;
    }
    std::string line;
    while (std::getline(f, line)) {
        size_t colon = line.find(':');
        if (colon == std::string::npos) continue;
        std::string key = line.substr(0, colon);
        std::string val = line.substr(colon + 1);
        auto trim = [](std::string& s) {
            s.erase(0, s.find_first_not_of(" \t\""));
            s.erase(s.find_last_not_of(" \t\"") + 1);
        };
        trim(val);
        if (key == "port") cfg.port = static_cast<uint16_t>(std::stoul(val));
        else if (key == "host") cfg.host = val;
        else if (key == "session_timeout_seconds") cfg.session_timeout_seconds = std::stoul(val);
        else if (key == "max_concurrent_sessions") cfg.max_concurrent_sessions = std::stoul(val);
        else if (key == "log_path") cfg.log_path = val;
    }
    return cfg;
}

int main(int argc, char* argv[]) {
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    std::string config_path = "~/.vault/relay.json";
    if (argc > 1) {
        std::string arg = argv[1];
        if (arg == "--config" && argc > 2)
            config_path = argv[2];
    }
    if (!config_path.empty() && config_path[0] == '~') {
        std::string home = std::getenv("HOME") ? std::getenv("HOME") : ".";
        config_path = home + config_path.substr(1);
    }
    RelayConfig cfg = loadConfig(config_path);
    if (!cfg.log_path.empty() && cfg.log_path[0] == '~') {
        std::string home = std::getenv("HOME") ? std::getenv("HOME") : ".";
        cfg.log_path = home + cfg.log_path.substr(1);
    }
    std::cout << "VAULT Relay starting on " << cfg.host << ":" << cfg.port << "\n";
    std::cout << "  Session timeout: " << cfg.session_timeout_seconds << "s\n";
    std::cout << "  Max sessions: " << cfg.max_concurrent_sessions << "\n";
    RelayServer relay(cfg);
    try {
        relay.start();
        while (g_running) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        relay.stop();
    } catch (const std::exception& e) {
        std::cerr << "Relay error: " << e.what() << "\n";
        return 1;
    }
    std::cout << "Relay stopped.\n";
    return 0;
}
