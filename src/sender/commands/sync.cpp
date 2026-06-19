#include "vault/object_store.hpp"
#include "vault/sic.hpp"
#include "vault/version_chain.hpp"
#include "vault/delta.hpp"
#include "vault/name_registry.hpp"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <sstream>
#include <iomanip>
#include <ctime>

namespace fs = std::filesystem;

struct LocalState {
    std::string head_path;
    std::string storage_path;
    LocalState() {
        std::string home = std::getenv("HOME") ? std::getenv("HOME") : ".";
        head_path = home + "/.vault/HEAD";
        storage_path = home + "/.vault/data";
        fs::create_directories(home + "/.vault");
    }
    std::string readHeadSic() {
        std::ifstream f(head_path);
        if (!f.is_open()) return "genesis";
        std::string sic;
        std::getline(f, sic);
        return sic.empty() ? "genesis" : sic;
    }
    void writeHeadSic(const std::string& sic) {
        std::ofstream f(head_path);
        f << sic << std::endl;
    }
};

int cmdSync(int argc, char* argv[]) {
    (void)argc; (void)argv;
    LocalState local;
    ObjectStore store(local.storage_path);
    NameRegistry names(local.storage_path);
    std::string local_sic = local.readHeadSic();
    auto versions = store.listVersions();
    std::vector<Version> new_versions;
    bool found = (local_sic == "genesis");
    for (const auto& v : versions) {
        if (found) new_versions.push_back(v);
        else if (v.sic == local_sic) found = true;
    }
    std::cout << "Syncing...\n";
    if (new_versions.empty()) {
        std::cout << "  Already up to date.\n";
        return 0;
    }
    std::cout << "  ✓ " << new_versions.size() << " new version(s) since your last sync\n\n";
    std::string server_head = names.getHead();
    bool conflicts = false;
    for (const auto& v : new_versions) {
        auto t = static_cast<time_t>(v.timestamp);
        std::cout << "  " << v.short_sic << "  " << v.name
                  << "  " << std::put_time(std::localtime(&t), "%Y-%m-%d %H:%M")
                  << "  " << v.author_id;
        if (v.sic == server_head) std::cout << "  ◄ HEAD";
        std::cout << "\n";
        Tree tree = store.loadTree(v.tree_hash);
        for (const auto& entry : tree.entries) {
            auto blob_data = store.loadBlob(entry.blob_hash);
            if (fs::exists(entry.name)) {
                std::ifstream existing(entry.name, std::ios::binary | std::ios::ate);
                size_t existing_size = existing.tellg();
                existing.seekg(0);
                std::vector<uint8_t> existing_data(existing_size);
                existing.read(reinterpret_cast<char*>(existing_data.data()), existing_size);
                std::string existing_hash = sha256Hex(std::string(existing_data.begin(), existing_data.end()));
                if (existing_hash != entry.blob_hash) {
                    std::cout << "  ⚠ Conflict detected in " << entry.name << "\n";
                    std::cout << "    Your local changes overlap with " << v.name << " (" << v.short_sic << ")\n";
                    std::cout << "    Resolve manually then run /HOOK to push your version.\n";
                    conflicts = true;
                }
            }
            if (!conflicts) {
                fs::create_directories(fs::path(entry.name).parent_path());
                std::ofstream f(entry.name, std::ios::binary);
                f.write(reinterpret_cast<const char*>(blob_data.data()), blob_data.size());
            }
        }
    }
    if (!conflicts) {
        local.writeHeadSic(server_head);
        names.setHead(server_head);
        for (const auto& v : new_versions) {
            if (!v.name.empty()) names.registerName(v.name, v.sic);
        }
        std::cout << "\n  Local files updated.\n";
    }
    if (conflicts) {
        std::cout << "\n  ⚠ Sync completed with conflicts. Resolve conflicts before pushing.\n";
        return 1;
    }
    return 0;
}
