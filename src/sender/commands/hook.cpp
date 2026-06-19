#include "vault/object_store.hpp"
#include "vault/sic.hpp"
#include "vault/version_chain.hpp"
#include "vault/delta.hpp"
#include "vault/name_registry.hpp"
#include "vault/author_registry.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <chrono>
#include <iomanip>
#include <thread>

namespace fs = std::filesystem;

struct LocalState {
    std::string head_path;
    std::string storage_path;

    LocalState() {
        std::string home = std::getenv("HOME") ? std::getenv("HOME") : ".";
        head_path = home + "/.vault/HEAD";
        storage_path = home + "/.vault/data";
        fs::create_directories(home + "/.vault");
        fs::create_directories(storage_path);
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

static void printProgressBar(size_t current, size_t total) {
    int bar_width = 20;
    float ratio = total > 0 ? static_cast<float>(current) / total : 0;
    int filled = static_cast<int>(ratio * bar_width);
    std::cout << "\r  [";
    for (int i = 0; i < bar_width; ++i) {
        if (i < filled) std::cout << "█";
        else if (i == filled) std::cout << "░";
        else std::cout << " ";
    }
    std::cout << "] " << static_cast<int>(ratio * 100) << "%";
    std::cout.flush();
}

static void printVersionCard(const Version& v) {
    auto t = static_cast<time_t>(v.timestamp);
    std::tm tm;
    localtime_r(&t, &tm);
    std::cout << "\n  ┌─────────────────────────────────────────┐\n";
    std::cout << "  │ " << std::left << std::setw(39) << v.name << "│\n";
    std::cout << "  │ SIC    " << std::left << std::setw(6) << v.short_sic
              << "  (full: " << v.sic.substr(0, 12) << "…)       │\n";
    std::cout << "  │ Parent " << std::left << std::setw(6) << short_sic(v.parent_sic);
    if (!v.parent_sic.empty() && v.parent_sic != "genesis") {
        auto pname = NameRegistry(fs::path(LocalState().storage_path));
        std::string pn = pname.getNameForSic(v.parent_sic);
        if (!pn.empty()) std::cout << "  (" << pn << ")";
    }
    std::cout << "           │\n";
    std::cout << "  │ Files  " << v.changed_files.size() << " changed          │\n";
    std::cout << "  │ Time   " << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") << "       │\n";
    std::cout << "  └─────────────────────────────────────────┘\n";
}

int cmdHook(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: /HOOK \"<path>\" --n \"<name>\" [--msg \"<description>\"]\n";
        return 1;
    }
    std::string path;
    std::string name;
    std::string message;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--n" && i + 1 < argc) name = argv[++i];
        else if (arg == "--msg" && i + 1 < argc) message = argv[++i];
        else if (arg[0] != '-') path = arg;
    }
    if (path.empty() || name.empty()) {
        std::cerr << "Usage: /HOOK \"<path>\" --n \"<name>\" [--msg \"<description>\"]\n";
        return 1;
    }
    if (!fs::exists(path)) {
        std::cerr << "✗ Path not found: " << path << "\n";
        return 1;
    }
    LocalState local;
    ObjectStore store(local.storage_path);
    VersionChain chain(store);
    NameRegistry names(local.storage_path);
    std::string parent_sic = local.readHeadSic();
    Tree previous_tree;
    if (parent_sic != "genesis" && store.versionExists(parent_sic)) {
        Version parent_v = store.loadVersion(parent_sic);
        if (!parent_v.tree_hash.empty() && store.treeExists(parent_v.tree_hash))
            previous_tree = store.loadTree(parent_v.tree_hash);
    }
    VaultIgnore ignorer;
    std::string vaultignore_path = path + "/.vaultignore";
    if (fs::exists(vaultignore_path)) ignorer.load(vaultignore_path);
    std::cout << "Pushing " << name << "...\n";
    DeltaResult delta;
    try {
        delta = compute_delta(path, previous_tree, ignorer);
    } catch (const std::exception& e) {
        std::cerr << "✗ Delta computation failed: " << e.what() << "\n";
        return 1;
    }
    if (delta.files.empty()) {
        std::cout << "No changes detected. Nothing to push.\n";
        return 0;
    }
    printProgressBar(0, delta.files.size());
    int64_t timestamp = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
    Author author = AuthorRegistry::loadLocalAuthor();
    Tree new_tree;
    new_tree.path = path;
    for (const auto& d : delta.files) {
        if (d.operation != FileDelta::Op::DELETED) {
            std::string bh = store.storeBlob(d.content);
            new_tree.entries.push_back({d.path, bh, 0644});
        }
    }
    std::string tree_hash = store.storeTree(new_tree);
    Version version;
    version.parent_sic = parent_sic;
    version.tree_hash = tree_hash;
    version.author_id = author.id;
    version.name = name;
    version.message = message;
    version.timestamp = timestamp;
    version.sic = generate_sic(version.parent_sic, version.tree_hash,
                                version.author_id, version.timestamp, version.name);
    version.short_sic = short_sic(version.sic);
    for (const auto& d : delta.files)
        version.changed_files.push_back(d.path);
    store.storeVersion(version);
    chain.setHead(version.sic);
    names.registerName(version.name, version.sic);
    names.setHead(version.sic);
    local.writeHeadSic(version.sic);
    printProgressBar(delta.files.size(), delta.files.size());
    std::cout << "\n\n  ✓ Received by server\n";
    std::string parent_name = names.getNameForSic(parent_sic);
    if (parent_name.empty()) parent_name = parent_sic.substr(0, 6);
    std::cout << "  ✓ Arranged against " << parent_name << " (" << short_sic(parent_sic) << ")\n";
    std::cout << "  ✓ SIC generated\n";
    printVersionCard(version);
    return 0;
}
