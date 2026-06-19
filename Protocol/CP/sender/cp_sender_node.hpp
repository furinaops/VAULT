#pragma once
#include "../shared/cp_config.hpp"
#include "../shared/cp_session.hpp"
#include <cstdint>
#include <string>
#include <array>
#include <thread>
#include <atomic>

class CPSenderNode {
public:
    CPSenderNode(const CPConfig& config, const std::array<uint8_t, 32>& login_key);
    ~CPSenderNode();
    void connect();   // does AUTH and SESSION_INIT, learns session id
    void disconnect();
    void send(DialogueRole role, const std::string& trigger, const std::string& params);
    bool isConnected() const;

private:
    void heartbeatLoop();
    CPConfig config_;
    uint16_t my_id_ = 0;
    std::array<uint8_t, 32> login_key_;
    CPSession session_;
    int socket_fd_ = -1;
    std::atomic<bool> running_{false};
    std::thread heartbeat_thread_;
    std::array<uint8_t, 32> session_key_;
};
