#include "vault/version_chain.hpp"
#include "vault/sic.hpp"
#include <fstream>
#include <filesystem>
#include <algorithm>
#include <iostream>

namespace fs = std::filesystem;

VersionChain::VersionChain(ObjectStore& store)
    : store_(store) {
    storage_path_ = fs::path(store_.storeBlob({}))
        .parent_path().parent_path().parent_path().string();
    if (storage_path_.empty()) {
        std::string home = std::getenv("HOME") ? std::getenv("HOME") : ".";
        storage_path_ = home + "/.vault/data";
    }
    fs::create_directories(storage_path_);
}

std::string VersionChain::headPath() const {
    auto p = fs::path(storage_path_);
    return p.parent_path().string() + "/HEAD";
}

std::string VersionChain::getHeadSic() const {
    std::string path = headPath();
    if (!fs::exists(path)) return "genesis";
    std::ifstream f(path);
    std::string sic;
    std::getline(f, sic);
    return sic.empty() ? "genesis" : sic;
}

void VersionChain::setHead(const std::string& sic) {
    fs::create_directories(fs::path(headPath()).parent_path());
    std::ofstream f(headPath());
    f << sic << std::endl;
}

Version VersionChain::getHead() const {
    std::string sic = getHeadSic();
    if (sic == "genesis" || !store_.versionExists(sic))
        return {};
    return store_.loadVersion(sic);
}

Version VersionChain::getBySic(const std::string& sic) const {
    if (!store_.versionExists(sic)) return {};
    return store_.loadVersion(sic);
}

Version VersionChain::getByName(const std::string& name) const {
    auto versions = store_.listVersions();
    for (const auto& v : versions) {
        if (v.name == name) return v;
    }
    return {};
}

Version VersionChain::append(const Version& version) {
    store_.storeVersion(version);
    setHead(version.sic);
    return version;
}

std::vector<Version> VersionChain::getHistory() const {
    return store_.listVersions();
}

std::vector<Version> VersionChain::getSince(const std::string& sic) const {
    auto all = store_.listVersions();
    if (sic == "genesis") return all;
    std::vector<Version> result;
    bool found = false;
    for (const auto& v : all) {
        if (found) {
            result.push_back(v);
        } else if (v.sic == sic) {
            found = true;
        }
    }
    return result;
}

size_t VersionChain::getLength() const {
    return store_.listVersions().size();
}

bool VersionChain::rollback(const std::string& sic) {
    if (!store_.versionExists(sic)) return false;
    setHead(sic);
    return true;
}

bool VersionChain::verifyIntegrity(const std::string& until_sic) const {
    auto all = store_.listVersions();
    if (all.empty()) return true;
    for (const auto& v : all) {
        std::string expected_sic = generate_sic(
            v.parent_sic,
            v.tree_hash,
            v.author_id,
            v.timestamp,
            v.name
        );
        if (expected_sic != v.sic) return false;
        if (!until_sic.empty() && v.sic == until_sic) break;
    }
    return true;
}
