#include "cp_config.hpp"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <stdexcept>
#include <cctype>
#include <optional>
#include <vector>

namespace {
static std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t");
    return s.substr(start, end - start + 1);
}
}

CPConfig CPConfig::loadFromFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) throw std::runtime_error("Cannot open config file");

    CPConfig config;
    std::string line;
    enum Section { NONE, SERVER, SESSION, AGENTS } section = NONE;

    auto parseValue = [&](const std::string& s) -> std::string {
        size_t pos = s.find(':');
        if (pos == std::string::npos) throw std::runtime_error("Missing :");
        std::string v = trim(s.substr(pos+1));
        if (v.size() >= 2 && v.front() == '"' && v.back() == '"')
            v = v.substr(1, v.size()-2);
        return v;
    };

    while (std::getline(file, line)) {
        line = trim(line);
        if (line.empty()) continue;
        if (line == "server:")  { section = SERVER; continue; }
        if (line == "session:") { section = SESSION; continue; }
        if (line == "agents:")  { section = AGENTS; continue; }

        if (section == SERVER) {
            if (line.find("host:") == 0) config.server_host = parseValue(line);
            else if (line.find("port:") == 0) config.server_port = static_cast<uint16_t>(std::stoul(parseValue(line)));
        } else if (section == SESSION) {
            if (line.find("version:") == 0) config.version = std::stoul(parseValue(line));
            else if (line.find("echo_expiry_ms:") == 0) config.echo_expiry_ms = std::stoul(parseValue(line));
            else if (line.find("echo_warn_ms:") == 0) config.echo_warn_ms = std::stoul(parseValue(line));
            else if (line.find("heartbeat_interval_ms:") == 0) config.heartbeat_interval_ms = std::stoul(parseValue(line));
            else if (line.find("liveness_timeout_ms:") == 0) config.liveness_timeout_ms = std::stoul(parseValue(line));
        } else if (section == AGENTS) {
            if (line.find("- id:") == 0) {
                AgentConfig agent;
                agent.id = static_cast<uint16_t>(std::stoul(parseValue(line), nullptr, 0));

                if (!std::getline(file, line)) break;
                line = trim(line);
                if (line.find("name:") != 0) throw std::runtime_error("Expected name");
                agent.name = parseValue(line);

                if (!std::getline(file, line)) break;
                line = trim(line);
                if (line.find("role:") != 0) throw std::runtime_error("Expected role");
                agent.role = parseValue(line);

                if (!std::getline(file, line)) break;
                line = trim(line);
                if (line.find("triggers:") != 0) throw std::runtime_error("Expected triggers");
                // triggers list
                if (line.find('[') != std::string::npos) {
                    // inline list
                    std::string lst = line.substr(line.find(':')+1);
                    lst.erase(std::remove(lst.begin(), lst.end(), '['), lst.end());
                    lst.erase(std::remove(lst.begin(), lst.end(), ']'), lst.end());
                    std::stringstream ss(lst);
                    std::string token;
                    while (std::getline(ss, token, ',')) {
                        token = trim(token);
                        if (!token.empty()) agent.triggers.insert(token);
                    }
                    config.agents.push_back(agent);
                } else {
                    // multiline list
                    while (std::getline(file, line)) {
                        line = trim(line);
                        if (line.empty()) continue;
                        if (line[0] == '-') {
                            // next agent
                            file.putback('\n');
                            for (auto it = line.rbegin(); it != line.rend(); ++it) file.putback(*it);
                            config.agents.push_back(agent);
                            goto next_agent;
                        }
                        std::string t = line;
                        if (t.front() == '-') t = t.substr(1);
                        t = trim(t);
                        if (!t.empty()) agent.triggers.insert(t);
                    }
                    config.agents.push_back(agent);
                    break;
                }
            next_agent: ;
            }
        }
    }
    return config;
}
