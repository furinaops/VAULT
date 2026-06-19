#pragma once
#include "../shared/cp_config.hpp"
#include "../shared/cp_session.hpp"
#include "../shared/cp_packet.hpp"
#include <string>
#include <functional>
#include <unordered_map>
#include <thread>
#include <atomic>
#include <array>

class CPReceiverNode {
public:
    CPReceiverNode(const CPConfig& config, const std::array<uint8_t, 32>& login_key);
    ~CPReceiverNode();
    void connect();
    void disconnect();
    void registerTrigger(const std::string& word, std::function<void(const std::string&)> fn);
    void run();
    void sendPacket(const CPPacket& pkt);

private:
    void heartbeatLoop();
    CPConfig config_;
    uint16_t my_id_ = 0;
    std::array<uint8_t, 32> login_key_;
    int socket_fd_ = -1;
    CPSession session_;
    std::atomic<bool> running_{false};
    std::thread heartbeat_thread_;
    std::array<uint8_t, 32> session_key_;
    std::unordered_map<std::string, std::function<void(const std::string&)>> registry_;
};
