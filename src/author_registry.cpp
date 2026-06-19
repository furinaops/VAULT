#include "vault/author_registry.hpp"
#include <fstream>
#include <filesystem>
#include <sstream>
#include <iostream>

namespace fs = std::filesystem;

AuthorRegistry::AuthorRegistry()
    : path_(localAuthorPath()) {
    load();
}

AuthorRegistry::AuthorRegistry(const std::string& config_path)
    : path_(config_path) {
    load();
}

std::string AuthorRegistry::localAuthorPath() {
    std::string home = std::getenv("HOME") ? std::getenv("HOME") : ".";
    return home + std::string("/.vault/author.json");
}

Author AuthorRegistry::loadLocalAuthor() {
    AuthorRegistry reg;
    auto all = reg.getAllAuthors();
    if (all.empty()) {
        Author a;
        a.id = "default";
        a.display_name = "Default User";
        a.public_key = "";
        return a;
    }
    return all.front();
}

void AuthorRegistry::registerAuthor(const Author& author) {
    authors_[author.id] = author;
    save();
}

Author AuthorRegistry::getAuthor(const std::string& id) const {
    auto it = authors_.find(id);
    if (it != authors_.end()) return it->second;
    return {};
}

bool AuthorRegistry::hasAuthor(const std::string& id) const {
    return authors_.find(id) != authors_.end();
}

std::vector<Author> AuthorRegistry::getAllAuthors() const {
    std::vector<Author> result;
    for (const auto& [id, author] : authors_)
        result.push_back(author);
    return result;
}

void AuthorRegistry::remove(const std::string& id) {
    authors_.erase(id);
    save();
}

void AuthorRegistry::save() const {
    fs::create_directories(fs::path(path_).parent_path());
    std::ofstream f(path_);
    f << "{\n";
    bool first = true;
    for (const auto& [id, author] : authors_) {
        if (!first) f << ",\n";
        first = false;
        f << "  \"" << id << "\": {\n"
          << "    \"id\": \"" << author.id << "\",\n"
          << "    \"display_name\": \"" << author.display_name << "\",\n"
          << "    \"public_key\": \"" << author.public_key << "\"\n"
          << "  }";
    }
    f << "\n}\n";
}

void AuthorRegistry::load() {
    authors_.clear();
    std::ifstream f(path_);
    if (!f.is_open()) return;
    std::string line;
    std::string current_id;
    while (std::getline(f, line)) {
        if (line.find('"') == std::string::npos) continue;
        if (line.find('{') != std::string::npos && line.find('"') != std::string::npos) {
            size_t q1 = line.find('"');
            size_t q2 = line.find('"', q1 + 1);
            if (q2 != std::string::npos) {
                current_id = line.substr(q1 + 1, q2 - q1 - 1);
                if (!current_id.empty() && current_id != "id" && current_id != "display_name" && current_id != "public_key") {
                    authors_[current_id] = {};
                    authors_[current_id].id = current_id;
                }
            }
        }
        if (current_id.empty()) continue;
        auto parseField = [&](const std::string& key) -> std::string {
            size_t pos = line.find('"' + key + '"');
            if (pos == std::string::npos) return "";
            size_t colon = line.find(':', pos);
            if (colon == std::string::npos) return "";
            size_t q1 = line.find('"', colon);
            if (q1 == std::string::npos) return "";
            size_t q2 = line.find('"', q1 + 1);
            if (q2 == std::string::npos) return "";
            return line.substr(q1 + 1, q2 - q1 - 1);
        };
        std::string v = parseField("display_name");
        if (!v.empty()) authors_[current_id].display_name = v;
        v = parseField("public_key");
        if (!v.empty()) authors_[current_id].public_key = v;
        v = parseField("id");
        if (!v.empty()) authors_[current_id].id = v;
    }
}
