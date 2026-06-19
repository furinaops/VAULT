#include "cp_server.hpp"
#include "../shared/cp_packet.hpp"
#include "../shared/cp_crypto.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <iostream>
#include <random>

CPServer::CPServer(const std::array<uint8_t, 32>& login_key, const CPConfig& config)
    : login_key_(login_key), config_(config) {
    for (const auto& agent : config_.agents)
        if (agent.role == "responder") expected_receivers_++;
}

CPServer::~CPServer() { stop(); }

void CPServer::start() {
    listen_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd_ < 0) throw std::runtime_error("socket() failed");
    int opt = 1;
    setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    fcntl(listen_fd_, F_SETFL, O_NONBLOCK);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(config_.server_port);
    inet_pton(AF_INET, config_.server_host.c_str(), &addr.sin_addr);
    if (bind(listen_fd_, (sockaddr*)&addr, sizeof(addr)) < 0)
        throw std::runtime_error("bind() failed");
    if (listen(listen_fd_, 10) < 0) throw std::runtime_error("listen() failed");

    running_ = true;
    worker_ = std::thread(&CPServer::eventLoop, this);
}

void CPServer::stop() {
    running_ = false;
    if (worker_.joinable()) worker_.join();
    if (listen_fd_ >= 0) close(listen_fd_);
}

void CPServer::waitForAllReceivers() {
    std::unique_lock lock(conn_mutex_);
    auth_cv_.wait(lock, [this]{ return authenticated_receivers_ >= expected_receivers_; });
}

void CPServer::eventLoop() {
    fd_set readfds;
    struct timeval tv;
    while (running_) {
        FD_ZERO(&readfds);
        FD_SET(listen_fd_, &readfds);
        int max_fd = listen_fd_;
        {
            std::lock_guard lock(conn_mutex_);
            for (auto& [fd, conn] : connections_) {
                FD_SET(fd, &readfds);
                if (fd > max_fd) max_fd = fd;
            }
        }
        tv.tv_sec = 0;
        tv.tv_usec = 500000; // 500 ms poll
        int ret = select(max_fd+1, &readfds, nullptr, nullptr, &tv);
        if (ret < 0 && running_) continue;
        if (ret > 0) {
            if (FD_ISSET(listen_fd_, &readfds)) handleNewConnection(listen_fd_);
            std::vector<int> ready;
            {
                std::lock_guard lock(conn_mutex_);
                for (auto& [fd, conn] : connections_)
                    if (FD_ISSET(fd, &readfds)) ready.push_back(fd);
            }
            for (int fd : ready) handleClientData(fd);
        }
        checkTimers();
    }
}

void CPServer::handleNewConnection(int listen_fd) {
    sockaddr_in peer{};
    socklen_t len = sizeof(peer);
    int fd = accept(listen_fd, (sockaddr*)&peer, &len);
    if (fd < 0) return;
    fcntl(fd, F_SETFL, O_NONBLOCK);

    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &peer.sin_addr, ip, sizeof(ip));

    Connection conn;
    conn.fd = fd;
    conn.authenticated = false;

    {
        std::lock_guard lock(conn_mutex_);
        connections_[fd] = conn;
    }
    std::cerr << "[server] new connection from " << ip << std::endl;
}

void CPServer::handleClientData(int fd) {
    uint8_t buf[4096];
    ssize_t n = recv(fd, buf, sizeof(buf), 0);
    if (n <= 0) {
        std::lock_guard lock(conn_mutex_);
        auto it = connections_.find(fd);
        if (it != connections_.end()) {
            if (it->second.agent_id > 0) agent_to_fd_.erase(it->second.agent_id);
            connections_.erase(it);
        }
        close(fd);
        return;
    }
    processPacket(fd, buf, static_cast<size_t>(n));
}

void CPServer::processPacket(int fd, const uint8_t* buf, size_t len) {
    if (len < 34) return;
    uint32_t crc_received;
    std::memcpy(&crc_received, buf + len - 4, 4);
    crc_received = ntohl(crc_received);
    if (CPPacket::computeCRC32(buf, len - 4) != crc_received) {
        std::cerr << "[server] CRC mismatch, dropping" << std::endl;
        return;
    }

    CPPacket pkt;
    try {
        pkt = CPPacket::deserialize(buf, len);
    } catch (const std::exception& e) {
        std::cerr << "[server] deserialize: " << e.what() << std::endl;
        return;
    }

    Connection* conn = nullptr;
    {
        std::lock_guard lock(conn_mutex_);
        auto it = connections_.find(fd);
        if (it == connections_.end()) return;
        conn = &it->second;
    }

    // Enforce AUTH on all non-authenticated connections
    if (!conn->authenticated) {
        if (pkt.role != DialogueRole::AUTH) {
            close(fd);
            std::lock_guard lock(conn_mutex_);
            connections_.erase(fd);
            return;
        }
        if (pkt.payload.size() != 32 ||
            std::memcmp(pkt.payload.data(), login_key_.data(), 32) != 0) {
            CPPacket fail; fail.role = DialogueRole::AUTH_FAIL;
            ::send(fd, fail.serialize().data(), fail.serialize().size(), MSG_NOSIGNAL);
            close(fd);
            std::lock_guard lock(conn_mutex_);
            connections_.erase(fd);
            return;
        }
        conn->authenticated = true;
        conn->agent_id = pkt.agent_id;
        {
            std::lock_guard lock(conn_mutex_);
            agent_to_fd_[pkt.agent_id] = fd;
        }
        CPPacket ok; ok.role = DialogueRole::AUTH_OK;
        ::send(fd, ok.serialize().data(), ok.serialize().size(), MSG_NOSIGNAL);

        // Notify if it's a responder
        for (const auto& agent : config_.agents)
            if (agent.id == pkt.agent_id && agent.role == "responder") {
                std::lock_guard lock(conn_mutex_);
                authenticated_receivers_++;
                auth_cv_.notify_all();
                break;
            }
        std::cerr << "[server] agent 0x" << std::hex << pkt.agent_id << " authenticated\n";
        return;
    }

    // Determine role of the authenticated agent
    std::string role;
    for (const auto& agent : config_.agents) {
        if (agent.id == conn->agent_id) {
            role = agent.role;
            break;
        }
    }

    // SESSION_INIT from initiator
    if (role == "initiator" && pkt.role == DialogueRole::SESSION_INIT && pkt.payload.size() == 32) {
        std::random_device rd;
        uint32_t sid = rd();
        SessionRecord rec;
        rec.session_id = sid;
        std::copy(pkt.payload.begin(), pkt.payload.end(), rec.session_key.begin());
        rec.node_ids.push_back(conn->agent_id); // initiator
        {
            std::lock_guard lock(session_mutex_);
            sessions_[sid] = rec;
        }
        
        CPPacket fwd = pkt;
        fwd.session_id = sid;
        
        std::vector<int> targets;
        {
            std::lock_guard lock(conn_mutex_);
            for (const auto& agent : config_.agents) {
                if (agent.role == "responder") {
                    auto it = agent_to_fd_.find(agent.id);
                    if (it != agent_to_fd_.end()) {
                        targets.push_back(it->second);
                        std::lock_guard slock(session_mutex_);
                        sessions_[sid].node_ids.push_back(agent.id);
                    }
                }
            }
            // Add initiator's fd
            targets.push_back(fd); 
        }

        auto fwd_data = fwd.serialize();
        for (int t_fd : targets) {
            ::send(t_fd, fwd_data.data(), fwd_data.size(), MSG_NOSIGNAL);
        }
        return;
    }

    // KEY_ECHO verification
    if (pkt.role == DialogueRole::KEY_ECHO) {
        if (pkt.payload.size() != 32) return;
        std::array<uint8_t, 32> echo;
        std::copy(pkt.payload.begin(), pkt.payload.end(), echo.begin());
        SessionRecord* rec = nullptr;
        {
            std::lock_guard lock(session_mutex_);
            auto it = sessions_.find(pkt.session_id);
            if (it == sessions_.end()) return;
            rec = &it->second;
        }
        auto expected = CPCrypto::computeEchoToken(rec->session_key, pkt.turn_id);
        if (echo != expected) {
            abortSession(pkt.session_id, AbortCause::KEY_MISMATCH);
            return;
        }
        std::lock_guard lock(session_mutex_);
        rec->echo_pending[pkt.agent_id] = false;
        return;
    }

    // ABORT forwarding
    if (pkt.role == DialogueRole::ABORT) {
        abortSession(pkt.session_id, static_cast<AbortCause>(pkt.payload.empty()?0:pkt.payload[0]));
        return;
    }

    // HEARTBEAT – just reset liveness (omitted for brevity)
    if (pkt.role == DialogueRole::HEARTBEAT) return;

    // All other dialogue packets: start echo timer and forward
    {
        std::lock_guard lock(session_mutex_);
        auto it = sessions_.find(pkt.session_id);
        if (it != sessions_.end()) {
            for (auto node_id : it->second.node_ids) {
                if (node_id != pkt.agent_id) {
                    it->second.echo_pending[node_id] = true;
                    it->second.turn_id_pending = pkt.turn_id;
                    it->second.echo_deadline = std::chrono::steady_clock::now() +
                        std::chrono::milliseconds(config_.echo_expiry_ms);
                    it->second.warn_deadline = std::chrono::steady_clock::now() +
                        std::chrono::milliseconds(config_.echo_warn_ms);
                }
            }
        }
    }
    sendToSession(pkt.session_id, {buf, buf+len}, pkt.agent_id);
}

void CPServer::sendToAgent(uint16_t agent_id, const std::vector<uint8_t>& data) {
    int fd = -1;
    {
        std::lock_guard lock(conn_mutex_);
        auto it = agent_to_fd_.find(agent_id);
        if (it != agent_to_fd_.end()) fd = it->second;
    }
    if (fd >= 0) {
        ::send(fd, data.data(), data.size(), MSG_NOSIGNAL);
    }
}

void CPServer::sendToSession(uint32_t session_id, const std::vector<uint8_t>& data, uint16_t exclude_agent) {
    std::vector<uint16_t> targets;
    {
        std::lock_guard lock(session_mutex_);
        auto it = sessions_.find(session_id);
        if (it == sessions_.end()) return;
        targets = it->second.node_ids;
    }
    for (auto node_id : targets) {
        if (node_id != exclude_agent)
            sendToAgent(node_id, data);
    }
}

void CPServer::checkTimers() {
    std::lock_guard lock(session_mutex_);
    auto now = std::chrono::steady_clock::now();
    for (auto it = sessions_.begin(); it != sessions_.end(); ) {
        auto& rec = it->second;
        if (rec.echo_pending.empty()) { ++it; continue; }
        if (now >= rec.echo_deadline) {
            abortSession(rec.session_id, AbortCause::ECHO_TIMEOUT);
            it = sessions_.erase(it);
            continue;
        }
        if (now >= rec.warn_deadline) {
            for (auto& [agent_id, pending] : rec.echo_pending) {
                if (pending) {
                    CPPacket pkt;
                    pkt.role = DialogueRole::BARTER_WARN;
                    pkt.session_id = rec.session_id;
                    pkt.turn_id = rec.turn_id_pending;
                    sendToAgent(agent_id, pkt.serialize());
                }
            }
            rec.warn_deadline = now + std::chrono::hours(1);
        }
        ++it;
    }
}

void CPServer::abortSession(uint32_t session_id, AbortCause cause) {
    CPPacket abort;
    abort.role = DialogueRole::ABORT;
    abort.session_id = session_id;
    abort.payload.push_back(static_cast<uint8_t>(cause));
    sendToSession(session_id, abort.serialize());
    std::lock_guard lock(session_mutex_);
    sessions_.erase(session_id);
}
