#include "cp_sender_node.hpp"
#include "../shared/cp_packet.hpp"
#include "../shared/cp_crypto.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <iostream>
#include <chrono>

CPSenderNode::CPSenderNode(const CPConfig& config, const std::array<uint8_t, 32>& login_key) 
    : config_(config), login_key_(login_key) {
    for (const auto& a : config_.agents) {
        if (a.role == "initiator") {
            my_id_ = a.id;
            break;
        }
    }
}

CPSenderNode::~CPSenderNode() { disconnect(); }

void CPSenderNode::connect() {
    socket_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd_ < 0) throw std::runtime_error("socket failed");
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(config_.server_port);
    inet_pton(AF_INET, config_.server_host.c_str(), &addr.sin_addr);
    if (::connect(socket_fd_, (sockaddr*)&addr, sizeof(addr)) < 0)
        throw std::runtime_error("connect failed");

    // Send AUTH first
    CPPacket auth;
    auth.role = DialogueRole::AUTH;
    auth.agent_id = my_id_;
    auth.payload.assign(login_key_.begin(), login_key_.end());
    auto auth_data = auth.serialize();
    ::send(socket_fd_, auth_data.data(), auth_data.size(), MSG_NOSIGNAL);

    uint8_t buf[4096];
    ssize_t n = recv(socket_fd_, buf, sizeof(buf), 0);
    if (n <= 0) throw std::runtime_error("No AUTH response");
    auto resp = CPPacket::deserialize(buf, n);
    if (resp.role != DialogueRole::AUTH_OK) throw std::runtime_error("AUTH failed");

    session_key_ = CPCrypto::generateSessionKey(my_id_, 0); // session id unknown yet

    CPPacket init;
    init.role = DialogueRole::SESSION_INIT;
    init.session_id = 0;
    init.turn_id = 0;
    init.agent_id = my_id_;
    init.payload.assign(session_key_.begin(), session_key_.end());
    auto data = init.serialize();
    ::send(socket_fd_, data.data(), data.size(), MSG_NOSIGNAL);

    // Wait for server reply with real session id
    n = recv(socket_fd_, buf, sizeof(buf), 0);
    if (n <= 0) throw std::runtime_error("No session init reply");
    auto pkt = CPPacket::deserialize(buf, n);
    if (pkt.role != DialogueRole::SESSION_INIT || pkt.payload.size() != 32)
        throw std::runtime_error("Invalid SESSION_INIT reply");

    session_.session_id = pkt.session_id;
    std::cout << "[sender node] new session id: 0x" << std::hex << pkt.session_id << std::endl;

    running_ = true;
    heartbeat_thread_ = std::thread(&CPSenderNode::heartbeatLoop, this);
}

void CPSenderNode::disconnect() {
    running_ = false;
    if (heartbeat_thread_.joinable()) heartbeat_thread_.join();
    if (socket_fd_ >= 0) close(socket_fd_);
    socket_fd_ = -1;
}

bool CPSenderNode::isConnected() const { return socket_fd_ >= 0; }

void CPSenderNode::send(DialogueRole role, const std::string& trigger, const std::string& params) {
    if (!isConnected()) return;
    if (role == DialogueRole::PROPOSE &&
        session_.state != SessionState::IDLE &&
        session_.state != SessionState::CONCLUDED &&
        session_.state != SessionState::ABORTED) {
        std::cerr << "Cannot propose in current state" << std::endl;
        return;
    }

    CPPacket pkt;
    pkt.role = role;
    pkt.session_id = session_.session_id;
    pkt.turn_id = session_.next_turn;
    session_.next_turn++;
    pkt.agent_id = my_id_;
    if (!trigger.empty()) std::strncpy(pkt.trigger.data(), trigger.c_str(), 15);
    if (!params.empty()) pkt.payload.assign(params.begin(), params.end());
    auto data = pkt.serialize();
    ::send(socket_fd_, data.data(), data.size(), MSG_NOSIGNAL);
}

void CPSenderNode::heartbeatLoop() {
    while (running_) {
        std::this_thread::sleep_for(std::chrono::milliseconds(config_.heartbeat_interval_ms));
        if (!running_) break;
        CPPacket hb;
        hb.role = DialogueRole::HEARTBEAT;
        hb.session_id = session_.session_id;
        hb.turn_id = 0;
        hb.agent_id = my_id_;
        auto d = hb.serialize();
        if (socket_fd_ >= 0) ::send(socket_fd_, d.data(), d.size(), MSG_NOSIGNAL);
    }
}
