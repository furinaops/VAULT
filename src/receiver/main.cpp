#include "receiver.hpp"
#include "cp_config.hpp"
#include "cp_crypto.hpp"
#include "cp_packet.hpp"
#include "cp_session.hpp"
#include "cp_receiver_node.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <csignal>
#include <atomic>

namespace fs = std::filesystem;

static std::atomic<bool> g_running{true};

void signalHandler(int) {
    g_running = false;
}

struct ReceiverConfig {
    uint16_t port = 9000;
    std::string storage_path = "~/.vault/repos/";
    std::string owner_key = "";
    std::string guest_key = "";
    size_t max_repo_size_mb = 2048;
    std::string log_path = "~/.vault/receiver.log";
    std::string cp_host = "127.0.0.1";
    uint16_t cp_port = 9001;
    std::string session_token = "";
};

static ReceiverConfig loadConfig(const std::string& path) {
    ReceiverConfig cfg;
    std::ifstream f(path);
    if (!f.is_open()) {
        std::cerr << "Warning: could not open config file " << path << ", using defaults\n";
        return cfg;
    }
    std::string line;
    while (std::getline(f, line)) {
        size_t colon = line.find(':');
        if (colon == std::string::npos) continue;
        std::string key = line.substr(0, colon);
        std::string val = line.substr(colon + 1);
        while (!val.empty() && (val.front() == ' ' || val.front() == '"'))
            val = val.substr(1);
        while (!val.empty() && (val.back() == ' ' || val.back() == '"' || val.back() == ','))
            val.pop_back();
        if (key == "port") cfg.port = static_cast<uint16_t>(std::stoul(val));
        else if (key == "storage_path") cfg.storage_path = val;
        else if (key == "owner_key") cfg.owner_key = val;
        else if (key == "guest_key") cfg.guest_key = val;
        else if (key == "max_repo_size_mb") cfg.max_repo_size_mb = std::stoul(val);
        else if (key == "log_path") cfg.log_path = val;
        else if (key == "cp_host") cfg.cp_host = val;
        else if (key == "cp_port") cfg.cp_port = static_cast<uint16_t>(std::stoul(val));
        else if (key == "session_token") cfg.session_token = val;
    }
    return cfg;
}

int main(int argc, char* argv[]) {
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    std::string config_path = "~/.vault/receiver.json";
    if (argc > 1) {
        std::string arg = argv[1];
        if (arg == "--config" && argc > 2)
            config_path = argv[2];
    }
    if (config_path[0] == '~') {
        std::string home = std::getenv("HOME") ? std::getenv("HOME") : ".";
        config_path = home + config_path.substr(1);
    }
    ReceiverConfig cfg = loadConfig(config_path);
    auto expandHome = [](std::string& path) {
        if (!path.empty() && path[0] == '~') {
            std::string home = std::getenv("HOME") ? std::getenv("HOME") : ".";
            path = home + path.substr(1);
        }
    };
    expandHome(cfg.storage_path);
    expandHome(cfg.log_path);
    fs::create_directories(cfg.storage_path + "/data");

    std::cout << "VAULT Receiver starting...\n";
    std::cout << "  Storage: " << cfg.storage_path << "\n";
    std::cout << "  Max size: " << cfg.max_repo_size_mb << " MB\n";

    CPConfig cp_cfg;
    cp_cfg.server_host = cfg.cp_host;
    cp_cfg.server_port = cfg.cp_port;
    AgentConfig agent;
    agent.id = 0x2001;
    agent.name = "VAULT Receiver";
    agent.role = "responder";
    agent.triggers = {"HOOK_INIT", "GET_REQ", "SIC_QUERY", "SYNC_REQ", "VERIFY_REQ", "ROLLBACK_REQ"};
    cp_cfg.agents.push_back(agent);

    auto login_key = CPCrypto::generateLoginKey();

    CPReceiverNode node(cp_cfg, login_key);

    VaultReceiver receiver(cfg.storage_path, cfg.owner_key, cfg.guest_key, cfg.max_repo_size_mb);

    node.registerTrigger("HOOK_INIT", [&](const std::string& params) {
        std::cout << "[receiver] HOOK_INIT: " << params.substr(0, 64) << "...\n";
        std::string resp = receiver.handleHookInit(params);
        CPPacket pkt;
        pkt.role = DialogueRole::INFORM;
        pkt.session_id = 0;
        pkt.agent_id = 0x2001;
        pkt.payload.assign(resp.begin(), resp.end());
        node.sendPacket(pkt);
    });

    node.registerTrigger("GET_REQ", [&](const std::string& params) {
        std::cout << "[receiver] GET_REQ: " << params.substr(0, 64) << "...\n";
        std::string resp = receiver.handleGetReq(params);
        CPPacket pkt;
        pkt.role = DialogueRole::INFORM;
        pkt.session_id = 0;
        pkt.agent_id = 0x2001;
        pkt.payload.assign(resp.begin(), resp.end());
        node.sendPacket(pkt);
    });

    node.registerTrigger("SIC_QUERY", [&](const std::string& params) {
        std::cout << "[receiver] SIC_QUERY\n";
        std::string resp = receiver.handleSicQuery(params);
        CPPacket pkt;
        pkt.role = DialogueRole::INFORM;
        pkt.session_id = 0;
        pkt.agent_id = 0x2001;
        pkt.payload.assign(resp.begin(), resp.end());
        node.sendPacket(pkt);
    });

    node.registerTrigger("SYNC_REQ", [&](const std::string& params) {
        std::cout << "[receiver] SYNC_REQ\n";
        std::string resp = receiver.handleSyncReq(params);
        CPPacket pkt;
        pkt.role = DialogueRole::INFORM;
        pkt.session_id = 0;
        pkt.agent_id = 0x2001;
        pkt.payload.assign(resp.begin(), resp.end());
        node.sendPacket(pkt);
    });

    node.registerTrigger("VERIFY_REQ", [&](const std::string& params) {
        std::cout << "[receiver] VERIFY_REQ\n";
        std::string resp = receiver.handleVerifyReq(params);
        CPPacket pkt;
        pkt.role = DialogueRole::INFORM;
        pkt.session_id = 0;
        pkt.agent_id = 0x2001;
        pkt.payload.assign(resp.begin(), resp.end());
        node.sendPacket(pkt);
    });

    node.registerTrigger("ROLLBACK_REQ", [&](const std::string& params) {
        std::cout << "[receiver] ROLLBACK_REQ\n";
        std::string resp = receiver.handleRollbackReq(params);
        CPPacket pkt;
        pkt.role = DialogueRole::INFORM;
        pkt.session_id = 0;
        pkt.agent_id = 0x2001;
        pkt.payload.assign(resp.begin(), resp.end());
        node.sendPacket(pkt);
    });

    try {
        node.connect();
        std::cout << "[receiver] Connected to CP server. Waiting for commands...\n";
        node.run();
    } catch (const std::exception& e) {
        std::cerr << "[receiver] Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
