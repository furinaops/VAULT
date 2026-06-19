#pragma once
#include "object_store.hpp"
#include <string>
#include <vector>

struct FileDelta {
    enum class Op { ADDED, MODIFIED, DELETED };
    Op operation;
    std::string path;
    std::string blob_hash;
    std::vector<uint8_t> content;
};

struct DeltaResult {
    std::vector<FileDelta> files;
    size_t total_insertions = 0;
    size_t total_deletions = 0;
};

struct IgnoreRule {
    std::string pattern;
    bool negate = false;
};

class VaultIgnore {
public:
    VaultIgnore();
    explicit VaultIgnore(const std::string& ignore_file);
    void load(const std::string& ignore_file);
    bool isIgnored(const std::string& path) const;
    void setDefaults();

private:
    std::vector<IgnoreRule> rules_;
    bool matches(const std::string& path, const std::string& pattern) const;
};

DeltaResult compute_delta(
    const std::string& current_path,
    const Tree& previous_tree,
    const VaultIgnore& ignorer
);

std::vector<uint8_t> pack_payload(const std::vector<FileDelta>& deltas, const Version& version);
std::pair<Version, std::vector<FileDelta>> unpack_payload(const std::vector<uint8_t>& data);

std::string loadFileToString(const std::string& path);
bool isBinaryFile(const std::string& path);
Tree buildTreeFromPath(const std::string& dir_path, const VaultIgnore& ignorer);
