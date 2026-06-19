#include "vault/object_store.hpp"
#include "vault/sic.hpp"
#include <fstream>
#include <filesystem>
#include <sstream>
#include <cstring>
#include <algorithm>

namespace fs = std::filesystem;

ObjectStore::ObjectStore(const std::string& storage_path)
    : storage_path_(storage_path) {
    fs::create_directories(storage_path_ + "/objects");
    fs::create_directories(storage_path_ + "/versions");
}

std::string ObjectStore::objectPath(const std::string& hash) const {
    if (hash.length() < 2) return storage_path_ + "/objects/" + hash;
    return storage_path_ + "/objects/" + hash.substr(0, 2) + "/" + hash.substr(2);
}

std::string ObjectStore::versionPath(const std::string& sic) const {
    return storage_path_ + "/versions/" + sic + ".json";
}

std::string ObjectStore::storeBlob(const std::vector<uint8_t>& data) {
    std::string hash = sha256Hex(std::string(data.begin(), data.end()));
    std::string path = objectPath(hash);
    if (fs::exists(path)) return hash;
    fs::create_directories(fs::path(path).parent_path());
    std::ofstream f(path, std::ios::binary);
    f.write(reinterpret_cast<const char*>(data.data()), data.size());
    return hash;
}

std::vector<uint8_t> ObjectStore::loadBlob(const std::string& hash) const {
    std::string path = objectPath(hash);
    if (!fs::exists(path)) return {};
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    size_t size = f.tellg();
    f.seekg(0);
    std::vector<uint8_t> data(size);
    f.read(reinterpret_cast<char*>(data.data()), size);
    return data;
}

bool ObjectStore::blobExists(const std::string& hash) const {
    return fs::exists(objectPath(hash));
}

std::string ObjectStore::storeTree(const Tree& tree) {
    std::ostringstream oss;
    for (const auto& entry : tree.entries) {
        oss << entry.name << '\0'
            << entry.blob_hash << '\0'
            << entry.permissions << '\n';
    }
    std::string serialized = oss.str();
    std::string hash = sha256Hex(serialized);
    std::string path = objectPath(hash);
    if (!fs::exists(path)) {
        fs::create_directories(fs::path(path).parent_path());
        std::ofstream f(path, std::ios::binary);
        f.write(serialized.data(), serialized.size());
    }
    return hash;
}

Tree ObjectStore::loadTree(const std::string& hash) const {
    std::string path = objectPath(hash);
    if (!fs::exists(path)) return {};
    std::ifstream f(path);
    Tree tree;
    tree.hash = hash;
    std::string line;
    while (std::getline(f, line)) {
        if (line.empty()) continue;
        size_t pos1 = line.find('\0');
        if (pos1 == std::string::npos) continue;
        size_t pos2 = line.find('\0', pos1 + 1);
        if (pos2 == std::string::npos) continue;
        TreeEntry entry;
        entry.name = line.substr(0, pos1);
        entry.blob_hash = line.substr(pos1 + 1, pos2 - pos1 - 1);
        entry.permissions = static_cast<uint32_t>(std::stoul(line.substr(pos2 + 1)));
        tree.entries.push_back(std::move(entry));
    }
    return tree;
}

bool ObjectStore::treeExists(const std::string& hash) const {
    return fs::exists(objectPath(hash));
}

std::string ObjectStore::storeVersion(const Version& version) {
    std::string path = versionPath(version.sic);
    fs::create_directories(fs::path(path).parent_path());
    std::ofstream f(path);
    f << "sic=" << version.sic << "\n"
      << "parent_sic=" << version.parent_sic << "\n"
      << "tree_hash=" << version.tree_hash << "\n"
      << "author_id=" << version.author_id << "\n"
      << "name=" << version.name << "\n"
      << "message=" << version.message << "\n"
      << "timestamp=" << version.timestamp << "\n"
      << "changed_files=";
    for (size_t i = 0; i < version.changed_files.size(); ++i) {
        if (i > 0) f << ",";
        f << version.changed_files[i];
    }
    f << "\n";
    return version.sic;
}

Version ObjectStore::loadVersion(const std::string& sic) const {
    std::string path = versionPath(sic);
    if (!fs::exists(path)) return {};
    std::ifstream f(path);
    Version v;
    v.sic = sic;
    v.short_sic = short_sic(sic);
    std::string line;
    while (std::getline(f, line)) {
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = line.substr(0, eq);
        std::string val = line.substr(eq + 1);
        if (key == "parent_sic") v.parent_sic = val;
        else if (key == "tree_hash") v.tree_hash = val;
        else if (key == "author_id") v.author_id = val;
        else if (key == "name") v.name = val;
        else if (key == "message") v.message = val;
        else if (key == "timestamp") v.timestamp = std::stoll(val);
        else if (key == "changed_files") {
            if (!val.empty()) {
                size_t pos = 0;
                while (pos < val.size()) {
                    size_t comma = val.find(',', pos);
                    if (comma == std::string::npos) {
                        v.changed_files.push_back(val.substr(pos));
                        break;
                    }
                    v.changed_files.push_back(val.substr(pos, comma - pos));
                    pos = comma + 1;
                }
            }
        }
    }
    return v;
}

bool ObjectStore::versionExists(const std::string& sic) const {
    return fs::exists(versionPath(sic));
}

std::vector<Version> ObjectStore::listVersions() const {
    std::vector<Version> versions;
    std::string dir = storage_path_ + "/versions";
    if (!fs::exists(dir)) return versions;
    for (const auto& entry : fs::directory_iterator(dir)) {
        if (entry.path().extension() == ".json") {
            std::string sic = entry.path().stem().string();
            versions.push_back(loadVersion(sic));
        }
    }
    std::sort(versions.begin(), versions.end(), [](const Version& a, const Version& b) {
        if (a.timestamp != b.timestamp) return a.timestamp < b.timestamp;
        return a.sic < b.sic;
    });
    return versions;
}

void ObjectStore::removeVersion(const std::string& sic) {
    std::string path = versionPath(sic);
    if (fs::exists(path)) fs::remove(path);
}
