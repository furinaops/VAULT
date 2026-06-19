#include "vault/name_registry.hpp"
#include <fstream>
#include <filesystem>
#include <sstream>
#include <algorithm>

namespace fs = std::filesystem;

NameRegistry::NameRegistry(const std::string& storage_path)
    : path_(storage_path) {
    fs::create_directories(path_);
    load();
}

std::string NameRegistry::filePath() const {
    return path_ + "/names.json";
}

void NameRegistry::save() const {
    std::ofstream f(filePath());
    f << "{\n";
    bool first = true;
    for (const auto& [name, sic] : names_) {
        if (!first) f << ",\n";
        first = false;
        f << "  \"" << name << "\": \"" << sic << "\"";
    }
    f << "\n}\n";
}

void NameRegistry::load() {
    names_.clear();
    std::ifstream f(filePath());
    if (!f.is_open()) {
        names_["HEAD"] = "";
        return;
    }
    std::string line;
    while (std::getline(f, line)) {
        if (line.find(':') == std::string::npos) continue;
        size_t quote1 = line.find('"');
        if (quote1 == std::string::npos) continue;
        size_t quote2 = line.find('"', quote1 + 1);
        if (quote2 == std::string::npos) continue;
        size_t colon = line.find(':', quote2);
        if (colon == std::string::npos) continue;
        size_t quote3 = line.find('"', colon);
        if (quote3 == std::string::npos) continue;
        size_t quote4 = line.find('"', quote3 + 1);
        if (quote4 == std::string::npos) continue;
        std::string name = line.substr(quote1 + 1, quote2 - quote1 - 1);
        std::string sic = line.substr(quote3 + 1, quote4 - quote3 - 1);
        names_[name] = sic;
    }
}

void NameRegistry::registerName(const std::string& name, const std::string& full_sic) {
    names_[name] = full_sic;
    save();
}

void NameRegistry::setHead(const std::string& full_sic) {
    names_["HEAD"] = full_sic;
    save();
}

std::string NameRegistry::resolve(const std::string& name_or_sic) const {
    if (name_or_sic == "latest") {
        auto it = names_.find("HEAD");
        if (it != names_.end()) return it->second;
        return "";
    }
    auto it = names_.find(name_or_sic);
    if (it != names_.end()) return it->second;
    if (name_or_sic.length() >= 6 && name_or_sic.length() <= 64) {
        for (const auto& [n, sic] : names_) {
            if (sic.substr(0, name_or_sic.length()) == name_or_sic)
                return sic;
            if (n == name_or_sic) return sic;
        }
        if (name_or_sic.length() == 64) return name_or_sic;
        for (const auto& [n, sic] : names_) {
            if (sic.find(name_or_sic) == 0) return sic;
        }
    }
    return name_or_sic;
}

std::string NameRegistry::getHead() const {
    auto it = names_.find("HEAD");
    if (it != names_.end()) return it->second;
    return "";
}

std::string NameRegistry::getNameForSic(const std::string& full_sic) const {
    for (const auto& [name, sic] : names_) {
        if (sic == full_sic) return name;
    }
    return "";
}

std::vector<std::pair<std::string, std::string>> NameRegistry::getAll() const {
    std::vector<std::pair<std::string, std::string>> result;
    for (const auto& [name, sic] : names_)
        result.push_back({name, sic});
    return result;
}

bool NameRegistry::hasName(const std::string& name) const {
    return names_.find(name) != names_.end();
}

void NameRegistry::remove(const std::string& name) {
    names_.erase(name);
    save();
}
