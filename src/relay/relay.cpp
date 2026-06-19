#include "vault/relay.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <iostream>
#include <algorithm>

RelayServer::RelayServer(const RelayConfig& config)
    : config_(config) {}

RelayServer::~RelayServer() { stop(); }

void RelayServer::start() {
    listen_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd_ < 0) throw std::runtime_error("socket() failed");
    int opt = 1;
    setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    fcntl(listen_fd_, F_SETFL, O_NONBLOCK);
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(config_.port);
    inet_pton(AF_INET, config_.host.c_str(), &addr.sin_addr);
    if (bind(listen_fd_, (sockaddr*)&addr, sizeof(addr)) < 0) {
        close(listen_fd_);
        throw std::runtime_error("bind() failed on port " + std::to_string(config_.port));
    }
    if (listen(listen_fd_, 10) < 0) {
        close(listen_fd_);
        throw std::runtime_error("listen() failed");
    }
    running_ = true;
    worker_ = std::thread(&RelayServer::eventLoop, this);
}

void RelayServer::stop() {
    running_ = false;
    if (worker_.joinable()) worker_.join();
    if (listen_fd_ >= 0) close(listen_fd_);
    listen_fd_ = -1;
    std::lock_guard lock(mutex_);
    for (auto& [fd, conn] : connections_)
        if (fd >= 0) close(fd);
    connections_.clear();
    sessions_.clear();
    token_to_fd_.clear();
}

void RelayServer::eventLoop() {
    while (running_) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(listen_fd_, &readfds);
        int max_fd = listen_fd_;
        {
            std::lock_guard lock(mutex_);
            for (auto& [fd, conn] : connections_) {
                FD_SET(fd, &readfds);
                if (fd > max_fd) max_fd = fd;
            }
        }
        struct timeval tv = {1, 0};
        int ret = select(max_fd + 1, &readfds, nullptr, nullptr, &tv);
        if (ret < 0) continue;
        if (ret > 0) {
            if (FD_ISSET(listen_fd_, &readfds))
                handleNewConnection(listen_fd_);
            std::vector<int> ready;
            {
                std::lock_guard lock(mutex_);
                for (auto& [fd, conn] : connections_)
                    if (FD_ISSET(fd, &readfds)) ready.push_back(fd);
            }
            for (int fd : ready) handleClientData(fd);
        }
        cleanupTimedOutSessions();
    }
}

void RelayServer::handleNewConnection(int listen_fd) {
    sockaddr_in peer{};
    socklen_t len = sizeof(peer);
    int fd = accept(listen_fd, (sockaddr*)&peer, &len);
    if (fd < 0) return;
    fcntl(fd, F_SETFL, O_NONBLOCK);
    auto now = std::chrono::steady_clock::now();
    std::lock_guard lock(mutex_);
    connections_[fd] = {fd, false, "", now, false, false, {}};
}

void RelayServer::handleClientData(int fd) {
    uint8_t buf[65536];
    ssize_t n = recv(fd, buf, sizeof(buf), 0);
    if (n <= 0) {
        std::lock_guard lock(mutex_);
        auto it = connections_.find(fd);
        if (it != connections_.end()) {
            std::string token = it->second.session_token;
            if (!token.empty()) {
                auto sit = sessions_.find(token);
                if (sit != sessions_.end()) {
                    int other_fd = (sit->second.receiver_fd == fd)
                        ? sit->second.sender_fd
                        : sit->second.receiver_fd;
                    if (other_fd >= 0) {
                        auto oit = connections_.find(other_fd);
                        if (oit != connections_.end()) {
                            oit->second.session_token.clear();
                            oit->second.registered = false;
                        }
                        close(other_fd);
                        connections_.erase(other_fd);
                    }
                    sessions_.erase(sit);
                }
                token_to_fd_.erase(token);
            }
            connections_.erase(it);
        }
        close(fd);
        return;
    }
    auto now = std::chrono::steady_clock::now();
    std::lock_guard lock(mutex_);
    auto conn_it = connections_.find(fd);
    if (conn_it == connections_.end()) return;
    auto& conn = conn_it->second;
    conn.last_activity = now;
    conn.read_buf.insert(conn.read_buf.end(), buf, buf + n);
    if (conn.read_buf.size() < 4) return;
    std::string msg_type(conn.read_buf.begin(), conn.read_buf.begin() + 4);
    if (msg_type == "REGI") {
        std::string token_str(conn.read_buf.begin() + 4, conn.read_buf.end());
        if (token_str.empty() || token_str.size() > 256) {
            const char* err = "ERR:invalid token";
            ::send(fd, err, strlen(err), MSG_NOSIGNAL);
            conn.read_buf.clear();
            return;
        }
        if (sessions_.size() >= config_.max_concurrent_sessions) {
            const char* err = "ERR:max sessions";
            ::send(fd, err, strlen(err), MSG_NOSIGNAL);
            conn.read_buf.clear();
            return;
        }
        conn.session_token = token_str;
        conn.is_receiver = true;
        conn.registered = true;
        RelaySession session;
        session.token = token_str;
        session.receiver_fd = fd;
        session.sender_fd = -1;
        session.created_at = now;
        session.last_activity = now;
        sessions_[token_str] = session;
        token_to_fd_[token_str] = fd;
        const char* ok = "REGI_OK";
        ::send(fd, ok, strlen(ok), MSG_NOSIGNAL);
        conn.read_buf.clear();
    } else if (msg_type == "JOIN") {
        std::string token_str(conn.read_buf.begin() + 4, conn.read_buf.end());
        auto sit = sessions_.find(token_str);
        if (sit == sessions_.end()) {
            const char* err = "ERR:session not found";
            ::send(fd, err, strlen(err), MSG_NOSIGNAL);
            conn.read_buf.clear();
            return;
        }
        conn.session_token = token_str;
        conn.is_receiver = false;
        conn.registered = true;
        sit->second.sender_fd = fd;
        sit->second.last_activity = now;
        token_to_fd_[token_str] = fd;
        const char* ok = "JOIN_OK";
        ::send(fd, ok, strlen(ok), MSG_NOSIGNAL);
        conn.read_buf.clear();
    } else if (conn.registered && !conn.session_token.empty()) {
        auto sit = sessions_.find(conn.session_token);
        if (sit == sessions_.end()) {
            conn.read_buf.clear();
            return;
        }
        sit->second.last_activity = now;
        int target_fd = conn.is_receiver
            ? sit->second.sender_fd
            : sit->second.receiver_fd;
        if (target_fd >= 0) {
            ::send(target_fd, conn.read_buf.data(), conn.read_buf.size(), MSG_NOSIGNAL);
        }
        conn.read_buf.clear();
    } else {
        conn.read_buf.clear();
        const char* err = "ERR:not registered";
        ::send(fd, err, strlen(err), MSG_NOSIGNAL);
    }
}

void RelayServer::cleanupTimedOutSessions() {
    auto now = std::chrono::steady_clock::now();
    auto timeout = std::chrono::seconds(config_.session_timeout_seconds);
    std::lock_guard lock(mutex_);
    for (auto it = sessions_.begin(); it != sessions_.end();) {
        if (now - it->second.last_activity > timeout) {
            if (it->second.receiver_fd >= 0) {
                close(it->second.receiver_fd);
                connections_.erase(it->second.receiver_fd);
            }
            if (it->second.sender_fd >= 0) {
                close(it->second.sender_fd);
                connections_.erase(it->second.sender_fd);
            }
            token_to_fd_.erase(it->second.token);
            it = sessions_.erase(it);
        } else {
            ++it;
        }
    }
}
