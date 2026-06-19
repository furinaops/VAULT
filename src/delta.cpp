#include "vault/delta.hpp"
#include "vault/sic.hpp"
#include <fstream>
#include <filesystem>
#include <sstream>
#include <cstring>
#include <iostream>
#include <arpa/inet.h>

#if defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__)
#include <libkern/OSByteOrder.h>
#define htobe64(x) OSSwapHostToBigInt64(x)
#define be64toh(x) OSSwapBigToHostInt64(x)
#elif defined(_WIN32)
#include <winsock2.h>
#define htobe64(x) htonll(x)
#define be64toh(x) ntohll(x)
#else
#include <endian.h>
#endif

namespace fs = std::filesystem;

VaultIgnore::VaultIgnore() {
    setDefaults();
}

VaultIgnore::VaultIgnore(const std::string& ignore_file) {
    setDefaults();
    load(ignore_file);
}

void VaultIgnore::setDefaults() {
    rules_.clear();
    rules_.push_back({".vault", false});
    rules_.push_back({".vaultignore", false});
    rules_.push_back({".git", false});
    rules_.push_back({".gitignore", false});
    rules_.push_back({"node_modules", false});
    rules_.push_back({"__pycache__", false});
    rules_.push_back({"*.pyc", false});
    rules_.push_back({".DS_Store", false});
    rules_.push_back({"*.swp", false});
    rules_.push_back({"*.swo", false});
    rules_.push_back({"*.bak", false});
}

void VaultIgnore::load(const std::string& ignore_file) {
    std::ifstream f(ignore_file);
    if (!f.is_open()) return;
    std::string line;
    while (std::getline(f, line)) {
        if (line.empty() || line[0] == '#') continue;
        bool negate = false;
        if (line[0] == '!') {
            negate = true;
            line = line.substr(1);
        }
        rules_.push_back({line, negate});
    }
}

bool VaultIgnore::matches(const std::string& path, const std::string& pattern) const {
    if (pattern == path) return true;
    if (path.find(pattern) != std::string::npos) return true;
    if (pattern.front() == '*') {
        std::string suffix = pattern.substr(1);
        if (path.size() >= suffix.size() &&
            path.compare(path.size() - suffix.size(), suffix.size(), suffix) == 0)
            return true;
    }
    if (pattern.back() == '*') {
        std::string prefix = pattern.substr(0, pattern.size() - 1);
        if (path.compare(0, prefix.size(), prefix) == 0) return true;
    }
    return false;
}

bool VaultIgnore::isIgnored(const std::string& path) const {
    bool ignored = false;
    for (const auto& rule : rules_) {
        if (matches(path, rule.pattern)) {
            ignored = !rule.negate;
        }
    }
    return ignored;
}

Tree buildTreeFromPath(const std::string& dir_path, const VaultIgnore& ignorer) {
    Tree tree;
    tree.path = dir_path;
    if (!fs::exists(dir_path)) return tree;
    for (const auto& entry : fs::recursive_directory_iterator(dir_path, fs::directory_options::skip_permission_denied)) {
        if (!fs::is_regular_file(entry)) continue;
        std::string rel = fs::relative(entry.path(), dir_path).string();
        if (ignorer.isIgnored(rel)) continue;
        auto perm = fs::status(entry.path()).permissions();
        uint32_t perms = 0;
        if ((perm & fs::perms::owner_read) != fs::perms::none) perms |= 0400;
        if ((perm & fs::perms::owner_write) != fs::perms::none) perms |= 0200;
        if ((perm & fs::perms::owner_exec) != fs::perms::none) perms |= 0100;
        if ((perm & fs::perms::group_read) != fs::perms::none) perms |= 0040;
        if ((perm & fs::perms::others_read) != fs::perms::none) perms |= 0004;
        std::ifstream f(entry.path(), std::ios::binary | std::ios::ate);
        size_t fsize = f.tellg();
        f.seekg(0);
        std::vector<uint8_t> data(fsize);
        f.read(reinterpret_cast<char*>(data.data()), fsize);
        std::string hash = sha256Hex(std::string(data.begin(), data.end()));
        tree.entries.push_back({rel, hash, perms});
    }
    std::string serialized;
    for (const auto& e : tree.entries) {
        serialized += e.name + '\0' + e.blob_hash + '\0' + std::to_string(e.permissions) + '\n';
    }
    tree.hash = sha256Hex(serialized);
    return tree;
}

DeltaResult compute_delta(
    const std::string& current_path,
    const Tree& previous_tree,
    const VaultIgnore& ignorer)
{
    DeltaResult result;
    Tree current_tree = buildTreeFromPath(current_path, ignorer);
    std::unordered_map<std::string, std::string> prev_map;
    for (const auto& e : previous_tree.entries)
        prev_map[e.name] = e.blob_hash;
    std::unordered_map<std::string, std::string> curr_map;
    for (const auto& e : current_tree.entries)
        curr_map[e.name] = e.blob_hash;
    for (const auto& [name, hash] : curr_map) {
        auto it = prev_map.find(name);
        if (it == prev_map.end()) {
            FileDelta delta;
            delta.operation = FileDelta::Op::ADDED;
            delta.path = name;
            delta.blob_hash = hash;
            std::string full_path = current_path + "/" + name;
            std::ifstream f(full_path, std::ios::binary | std::ios::ate);
            size_t fsize = f.tellg();
            f.seekg(0);
            delta.content.resize(fsize);
            f.read(reinterpret_cast<char*>(delta.content.data()), fsize);
            result.total_insertions += fsize;
            result.files.push_back(std::move(delta));
        } else if (it->second != hash) {
            FileDelta delta;
            delta.operation = FileDelta::Op::MODIFIED;
            delta.path = name;
            delta.blob_hash = hash;
            std::string full_path = current_path + "/" + name;
            std::ifstream f(full_path, std::ios::binary | std::ios::ate);
            size_t fsize = f.tellg();
            f.seekg(0);
            delta.content.resize(fsize);
            f.read(reinterpret_cast<char*>(delta.content.data()), fsize);
            result.total_insertions += fsize;
            result.files.push_back(std::move(delta));
        }
    }
    for (const auto& [name, hash] : prev_map) {
        if (curr_map.find(name) == curr_map.end()) {
            FileDelta delta;
            delta.operation = FileDelta::Op::DELETED;
            delta.path = name;
            result.total_deletions++;
            result.files.push_back(std::move(delta));
        }
    }
    return result;
}

std::vector<uint8_t> pack_payload(const std::vector<FileDelta>& deltas, const Version& version) {
    std::vector<uint8_t> payload;
    auto appendStr = [&](const std::string& s) {
        uint32_t len = htonl(static_cast<uint32_t>(s.size()));
        payload.insert(payload.end(), reinterpret_cast<uint8_t*>(&len),
                       reinterpret_cast<uint8_t*>(&len) + 4);
        payload.insert(payload.end(), s.begin(), s.end());
    };
    auto appendU64 = [&](uint64_t v) {
        uint64_t nv = htobe64(v);
        payload.insert(payload.end(), reinterpret_cast<uint8_t*>(&nv),
                       reinterpret_cast<uint8_t*>(&nv) + 8);
    };
    auto appendU32 = [&](uint32_t v) {
        uint32_t nv = htonl(v);
        payload.insert(payload.end(), reinterpret_cast<uint8_t*>(&nv),
                       reinterpret_cast<uint8_t*>(&nv) + 4);
    };
    appendStr(version.sic);
    appendStr(version.parent_sic);
    appendStr(version.tree_hash);
    appendStr(version.author_id);
    appendStr(version.name);
    appendStr(version.message);
    appendU64(static_cast<uint64_t>(version.timestamp));
    appendU32(static_cast<uint32_t>(version.changed_files.size()));
    for (const auto& cf : version.changed_files)
        appendStr(cf);
    appendU32(static_cast<uint32_t>(deltas.size()));
    for (const auto& d : deltas) {
        appendU32(static_cast<uint32_t>(d.operation));
        appendStr(d.path);
        appendStr(d.blob_hash);
        appendU32(static_cast<uint32_t>(d.content.size()));
        payload.insert(payload.end(), d.content.begin(), d.content.end());
    }
    return payload;
}

std::pair<Version, std::vector<FileDelta>> unpack_payload(const std::vector<uint8_t>& data) {
    Version v;
    std::vector<FileDelta> deltas;
    size_t pos = 0;
    auto readStr = [&]() -> std::string {
        if (pos + 4 > data.size()) return "";
        uint32_t len;
        std::memcpy(&len, data.data() + pos, 4);
        len = ntohl(len);
        pos += 4;
        if (pos + len > data.size()) return "";
        std::string s(data.begin() + pos, data.begin() + pos + len);
        pos += len;
        return s;
    };
    auto readU64 = [&]() -> uint64_t {
        if (pos + 8 > data.size()) return 0;
        uint64_t v;
        std::memcpy(&v, data.data() + pos, 8);
        pos += 8;
        return be64toh(v);
    };
    auto readU32 = [&]() -> uint32_t {
        if (pos + 4 > data.size()) return 0;
        uint32_t v;
        std::memcpy(&v, data.data() + pos, 4);
        pos += 4;
        return ntohl(v);
    };
    v.sic = readStr();
    v.parent_sic = readStr();
    v.tree_hash = readStr();
    v.author_id = readStr();
    v.name = readStr();
    v.message = readStr();
    v.timestamp = static_cast<int64_t>(readU64());
    v.short_sic = short_sic(v.sic);
    uint32_t num_cf = readU32();
    for (uint32_t i = 0; i < num_cf; ++i)
        v.changed_files.push_back(readStr());
    uint32_t num_deltas = readU32();
    for (uint32_t i = 0; i < num_deltas; ++i) {
        FileDelta d;
        d.operation = static_cast<FileDelta::Op>(readU32());
        d.path = readStr();
        d.blob_hash = readStr();
        uint32_t content_len = readU32();
        if (content_len > 0 && pos + content_len <= data.size()) {
            d.content.assign(data.begin() + pos, data.begin() + pos + content_len);
            pos += content_len;
        }
        deltas.push_back(std::move(d));
    }
    return {v, deltas};
}

std::string loadFileToString(const std::string& path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f.is_open()) return "";
    size_t size = f.tellg();
    f.seekg(0);
    std::string content(size, '\0');
    f.read(&content[0], size);
    return content;
}

bool isBinaryFile(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open()) return false;
    char buf[8192];
    f.read(buf, sizeof(buf));
    size_t n = f.gcount();
    for (size_t i = 0; i < n; ++i) {
        unsigned char c = static_cast<unsigned char>(buf[i]);
        if (c == 0) return true;
    }
    return false;
}
