#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>
#include <set>

struct RelayConfig {
    uint16_t port = 9001;
    uint32_t session_timeout_seconds = 3600;
    uint32_t max_concurrent_sessions = 50;
    std::string log_path = "~/.vault/relay.log";
    std::string host = "0.0.0.0";
    uint32_t max_msg_size = 67108864;
};

class RelayServer {
public:
    explicit RelayServer(const RelayConfig& config);
    ~RelayServer();

    void start();
    void stop();

private:
    void eventLoop();
    void handleNewConnection(int listen_fd);
    void handleClientData(int fd);
    void cleanupTimedOutSessions();

    RelayConfig config_;
    int listen_fd_ = -1;
    std::thread worker_;
    std::atomic<bool> running_{false};

    struct RelayConnection {
        int fd = -1;
        bool registered = false;
        std::string session_token;
        std::chrono::steady_clock::time_point last_activity;
        bool is_receiver = false;
        bool authenticated = false;
        std::vector<uint8_t> read_buf;
    };

    struct RelaySession {
        std::string token;
        int receiver_fd = -1;
        int sender_fd = -1;
        std::chrono::steady_clock::time_point created_at;
        std::chrono::steady_clock::time_point last_activity;
    };

    std::mutex mutex_;
    std::map<int, RelayConnection> connections_;
    std::map<std::string, RelaySession> sessions_;
    std::map<std::string, int> token_to_fd_;
};
