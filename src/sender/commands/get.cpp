#include "vault/object_store.hpp"
#include "vault/sic.hpp"
#include "vault/version_chain.hpp"
#include "vault/delta.hpp"
#include "vault/name_registry.hpp"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <sstream>

namespace fs = std::filesystem;

struct LocalState {
    std::string head_path;
    std::string storage_path;
    LocalState() {
        std::string home = std::getenv("HOME") ? std::getenv("HOME") : ".";
        head_path = home + "/.vault/HEAD";
        storage_path = home + "/.vault/data";
    }
    void writeHeadSic(const std::string& sic) {
        std::ofstream f(head_path);
        f << sic << std::endl;
    }
};

int cmdGet(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: /GET \"<sic_or_name_or_latest>\" [--delta] [--log]\n";
        return 1;
    }
    std::string target = argv[1];
    bool show_log = false;
    bool use_delta = false;
    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--log") show_log = true;
        if (arg == "--delta") use_delta = true;
    }
    LocalState local;
    ObjectStore store(local.storage_path);
    NameRegistry names(local.storage_path);
    std::string full_sic;
    if (target == "latest" || target.empty())
        full_sic = names.getHead();
    else
        full_sic = names.resolve(target);
    if (full_sic.empty() || !store.versionExists(full_sic)) {
        std::cerr << "✗ Version not found: \"" << target << "\"\n";
        std::cerr << "  No version with that name or SIC exists.\n";
        std::cerr << "  Run /SIC --log to see available versions.\n";
        return 1;
    }
    Version v = store.loadVersion(full_sic);
    Tree tree = store.loadTree(v.tree_hash);
    if (!use_delta) {
        for (const auto& entry : tree.entries) {
            auto blob_data = store.loadBlob(entry.blob_hash);
            fs::create_directories(fs::path(entry.name).parent_path());
            std::ofstream f(entry.name, std::ios::binary);
            f.write(reinterpret_cast<const char*>(blob_data.data()), blob_data.size());
            std::cout << "  Restored: " << entry.name << "\n";
        }
    }
    local.writeHeadSic(full_sic);
    names.setHead(full_sic);
    if (!v.name.empty()) names.registerName(v.name, full_sic);
    std::cout << "✓ Fetched version \"" << v.name << "\" (" << v.short_sic << ")\n";
    if (show_log) {
        std::cout << "\n  Version chain:\n";
        auto versions = store.listVersions();
        for (const auto& ver : versions) {
            std::cout << "  " << ver.short_sic << "  " << ver.name;
            if (ver.sic == full_sic) std::cout << "  ◄ current";
            std::cout << "\n";
        }
    }
    return 0;
}
