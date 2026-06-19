#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <unordered_map>

struct Blob {
    std::string hash;
    std::vector<uint8_t> data;
    size_t size;
};

struct TreeEntry {
    std::string name;
    std::string blob_hash;
    uint32_t permissions;
};

struct Tree {
    std::string hash;
    std::vector<TreeEntry> entries;
    std::string path;
};

struct Version {
    std::string sic;
    std::string short_sic;
    std::string parent_sic;
    std::string tree_hash;
    std::string author_id;
    std::string name;
    std::string message;
    int64_t timestamp;
    std::vector<std::string> changed_files;
};

class ObjectStore {
public:
    explicit ObjectStore(const std::string& storage_path);

    std::string storeBlob(const std::vector<uint8_t>& data);
    std::vector<uint8_t> loadBlob(const std::string& hash) const;
    bool blobExists(const std::string& hash) const;

    std::string storeTree(const Tree& tree);
    Tree loadTree(const std::string& hash) const;
    bool treeExists(const std::string& hash) const;

    std::string storeVersion(const Version& version);
    Version loadVersion(const std::string& sic) const;
    bool versionExists(const std::string& sic) const;
    std::vector<Version> listVersions() const;

    void removeVersion(const std::string& sic);

private:
    std::string storage_path_;
    std::string objectPath(const std::string& hash) const;
    std::string versionPath(const std::string& sic) const;
};
