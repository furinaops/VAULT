#include "cp_receiver_node.hpp"
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
#include <stdexcept>

CPReceiverNode::CPReceiverNode(const CPConfig& config, const std::array<uint8_t, 32>& login_key)
    : config_(config), login_key_(login_key) {
    for (const auto& a : config_.agents) {
        if (a.role == "responder") {
            my_id_ = a.id;
            break;
        }
    }
}

CPReceiverNode::~CPReceiverNode() { disconnect(); }

void CPReceiverNode::connect() {
    socket_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd_ < 0) throw std::runtime_error("socket failed");
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(config_.server_port);
    inet_pton(AF_INET, config_.server_host.c_str(), &addr.sin_addr);
    if (::connect(socket_fd_, (sockaddr*)&addr, sizeof(addr)) < 0)
        throw std::runtime_error("connect failed");

    // Send AUTH
    CPPacket auth;
    auth.role = DialogueRole::AUTH;
    auth.agent_id = my_id_;
    auth.payload.assign(login_key_.begin(), login_key_.end());
    auto data = auth.serialize();
    ::send(socket_fd_, data.data(), data.size(), MSG_NOSIGNAL);

    uint8_t buf[4096];
    ssize_t n = recv(socket_fd_, buf, sizeof(buf), 0);
    if (n <= 0) throw std::runtime_error("No AUTH response");
    auto resp = CPPacket::deserialize(buf, n);
    if (resp.role == DialogueRole::AUTH_FAIL) {
        std::cerr << "Authentication failed. Wrong key." << std::endl;
        exit(1);
    } else if (resp.role != DialogueRole::AUTH_OK) {
        throw std::runtime_error("Unexpected AUTH response");
    }
    std::cout << "Authenticated. Connected to server.\n";

    // Expect SESSION_INIT
    n = recv(socket_fd_, buf, sizeof(buf), 0);
    if (n <= 0) throw std::runtime_error("No SESSION_INIT");
    resp = CPPacket::deserialize(buf, n);
    if (resp.role != DialogueRole::SESSION_INIT || resp.payload.size() != 32)
        throw std::runtime_error("Invalid SESSION_INIT");
    std::copy(resp.payload.begin(), resp.payload.end(), session_key_.begin());
    session_.session_id = resp.session_id;
    std::cout << "Session key received, session id: 0x" << std::hex << resp.session_id << std::endl;

    running_ = true;
    heartbeat_thread_ = std::thread(&CPReceiverNode::heartbeatLoop, this);
}

void CPReceiverNode::disconnect() {
    running_ = false;
    if (heartbeat_thread_.joinable()) heartbeat_thread_.join();
    if (socket_fd_ >= 0) close(socket_fd_);
    socket_fd_ = -1;
}

void CPReceiverNode::registerTrigger(const std::string& word, std::function<void(const std::string&)> fn) {
    registry_[word] = fn;
}

void CPReceiverNode::heartbeatLoop() {
    while (running_) {
        std::this_thread::sleep_for(std::chrono::milliseconds(config_.heartbeat_interval_ms));
        if (!running_) break;
        CPPacket hb;
        hb.role = DialogueRole::HEARTBEAT;
        hb.session_id = session_.session_id;
        hb.turn_id = 0;
        hb.agent_id = config_.agents.front().id;
        auto d = hb.serialize();
        if (socket_fd_ >= 0) ::send(socket_fd_, d.data(), d.size(), MSG_NOSIGNAL);
    }
}

void CPReceiverNode::sendPacket(const CPPacket& pkt) {
    auto data = pkt.serialize();
    ::send(socket_fd_, data.data(), data.size(), MSG_NOSIGNAL);
}

void CPReceiverNode::run() {
    std::cout << "Waiting for session...\n";
    while (running_) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(socket_fd_, &readfds);
        struct timeval tv = {1, 0};
        int ret = select(socket_fd_+1, &readfds, nullptr, nullptr, &tv);
        if (ret < 0) break;
        if (ret == 0) continue;
        uint8_t buf[4096];
        ssize_t n = recv(socket_fd_, buf, sizeof(buf), 0);
        if (n <= 0) { running_ = false; break; }
        if (n < 34) continue;

        uint32_t crc_received;
        std::memcpy(&crc_received, buf + n - 4, 4);
        crc_received = ntohl(crc_received);
        if (CPPacket::computeCRC32(buf, n - 4) != crc_received) {
            std::cerr << "[receiver] CRC mismatch, dropping\n";
            continue;
        }
        CPPacket pkt;
        try { pkt = CPPacket::deserialize(buf, n); }
        catch (...) { continue; }

        if (pkt.role == DialogueRole::HEARTBEAT) continue;
        if (pkt.role == DialogueRole::KEY_ECHO) continue;

        // Send KEY_ECHO for all non-exempt packets
        if (pkt.role != DialogueRole::AUTH && pkt.role != DialogueRole::AUTH_OK &&
            pkt.role != DialogueRole::AUTH_FAIL && pkt.role != DialogueRole::SESSION_INIT &&
            pkt.role != DialogueRole::HEARTBEAT && pkt.role != DialogueRole::KEY_ECHO) {
            CPPacket echo;
            echo.role = DialogueRole::KEY_ECHO;
            echo.session_id = pkt.session_id;
            echo.turn_id = pkt.turn_id;
            echo.agent_id = config_.agents.front().id;
            auto token = CPCrypto::computeEchoToken(session_key_, pkt.turn_id);
            echo.payload.assign(token.begin(), token.end());
            sendPacket(echo);
        }

        // Turn monotonic enforcement (dialogue only)
        if (pkt.turn_id != 0 &&
            pkt.role != DialogueRole::AUTH && pkt.role != DialogueRole::AUTH_OK &&
            pkt.role != DialogueRole::AUTH_FAIL && pkt.role != DialogueRole::SESSION_INIT &&
            pkt.role != DialogueRole::HEARTBEAT && pkt.role != DialogueRole::KEY_ECHO) {
            if (pkt.turn_id <= session_.last_seen_turn) {
                CPPacket abort;
                abort.role = DialogueRole::ABORT;
                abort.session_id = session_.session_id;
                abort.turn_id = session_.next_turn;
                abort.agent_id = config_.agents.front().id;
                abort.payload.push_back(static_cast<uint8_t>(AbortCause::TURN_MISMATCH));
                sendPacket(abort);
                session_.abort(AbortCause::TURN_MISMATCH);
                continue;
            }
            session_.last_seen_turn = pkt.turn_id;
            session_.next_turn = pkt.turn_id + 1;
        }

        if (!session_.processIncoming(pkt.role)) {
            CPPacket abort;
            abort.role = DialogueRole::ABORT;
            abort.session_id = session_.session_id;
            abort.turn_id = session_.next_turn;
            abort.agent_id = config_.agents.front().id;
            abort.payload.push_back(static_cast<uint8_t>(AbortCause::INVALID_TRANSITION));
            sendPacket(abort);
            session_.abort(AbortCause::INVALID_TRANSITION);
            continue;
        }

        // State‑specific actions
        switch (pkt.role) {
        case DialogueRole::PROPOSE: {
            std::string trig = pkt.getTriggerWord();
            const auto& my_agent = config_.agents.front();
            if (my_agent.triggers.find(trig) == my_agent.triggers.end() ||
                registry_.find(trig) == registry_.end()) {
                CPPacket abort;
                abort.role = DialogueRole::ABORT;
                abort.session_id = session_.session_id;
                abort.turn_id = session_.next_turn;
                abort.agent_id = my_agent.id;
                abort.payload.push_back(static_cast<uint8_t>(AbortCause::UNKNOWN_TRIGGER));
                sendPacket(abort);
                session_.abort(AbortCause::UNKNOWN_TRIGGER);
                continue;
            }
            session_.stored_trigger_word = trig;
            session_.stored_trigger_params.assign(pkt.payload.begin(), pkt.payload.end());
            CPPacket accept;
            accept.role = DialogueRole::ACCEPT;
            accept.session_id = session_.session_id;
            accept.turn_id = session_.next_turn++;
            accept.agent_id = my_agent.id;
            sendPacket(accept);
            break;
        }
        case DialogueRole::COMMIT: {
            try {
                registry_[session_.stored_trigger_word](session_.stored_trigger_params);
            } catch (...) {
                CPPacket abort;
                abort.role = DialogueRole::ABORT;
                abort.session_id = session_.session_id;
                abort.turn_id = session_.next_turn;
                abort.agent_id = config_.agents.front().id;
                abort.payload.push_back(static_cast<uint8_t>(AbortCause::TRIGGER_FAILED));
                sendPacket(abort);
                session_.abort(AbortCause::TRIGGER_FAILED);
                continue;
            }
            CPPacket progress;
            progress.role = DialogueRole::PROGRESS;
            progress.session_id = session_.session_id;
            progress.turn_id = session_.next_turn++;
            progress.agent_id = config_.agents.front().id;
            progress.payload = {'5','0','%'};
            sendPacket(progress);

            CPPacket conclude;
            conclude.role = DialogueRole::CONCLUDE;
            conclude.session_id = session_.session_id;
            conclude.turn_id = session_.next_turn++;
            conclude.agent_id = config_.agents.front().id;
            sendPacket(conclude);
            break;
        }
        default: ;
        }
    }
}
