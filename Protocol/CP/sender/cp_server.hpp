#pragma once
#include "../shared/cp_config.hpp"
#include "../shared/cp_types.hpp"
#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <array>
#include <sys/select.h>

struct SessionRecord {
    uint32_t session_id = 0;
    std::array<uint8_t, 32> session_key{};
    std::vector<uint16_t> node_ids;
    std::map<uint16_t, bool> echo_pending;            // agent_id -> waiting
    std::chrono::steady_clock::time_point echo_deadline;
    std::chrono::steady_clock::time_point warn_deadline;
    uint32_t turn_id_pending = 0;
};

class CPServer {
public:
    CPServer(const std::array<uint8_t, 32>& login_key, const CPConfig& config);
    ~CPServer();

    void start();
    void stop();
    void waitForAllReceivers();

private:
    void eventLoop();
    void handleNewConnection(int listen_fd);
    void handleClientData(int fd);
    void processPacket(int fd, const uint8_t* buf, size_t len);
    void sendToAgent(uint16_t agent_id, const std::vector<uint8_t>& data);
    void sendToSession(uint32_t session_id, const std::vector<uint8_t>& data, uint16_t exclude_agent = 0);
    void checkTimers();
    void abortSession(uint32_t session_id, AbortCause cause);

    std::array<uint8_t, 32> login_key_;
    CPConfig config_;
    int listen_fd_ = -1;
    std::thread worker_;
    bool running_ = false;

    struct Connection {
        int fd = -1;
        uint16_t agent_id = 0;
        bool authenticated = false;
        bool is_initiator = false;
    };
    std::mutex conn_mutex_;
    std::map<int, Connection> connections_;
    std::map<uint16_t, int> agent_to_fd_;

    std::recursive_mutex session_mutex_;
    std::map<uint32_t, SessionRecord> sessions_;

    size_t authenticated_receivers_ = 0;
    size_t expected_receivers_ = 0;
    std::condition_variable auth_cv_;
};
