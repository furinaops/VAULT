#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <set>

struct AgentConfig {
    uint16_t id = 0;
    std::string name;
    std::string role;                     // "initiator" or "responder"
    std::set<std::string> triggers;
};

struct CPConfig {
    std::string server_host = "127.0.0.1";
    uint16_t server_port = 9000;

    uint32_t version = 1;
    uint32_t echo_expiry_ms = 10000;
    uint32_t echo_warn_ms = 7000;
    uint32_t heartbeat_interval_ms = 500;
    uint32_t liveness_timeout_ms = 30000;

    std::vector<AgentConfig> agents;

    static CPConfig loadFromFile(const std::string& path);
    static CPConfig loadDefault(const std::string& argv0 = "");
    static std::string projectPath(const std::string& relative_path);
};
