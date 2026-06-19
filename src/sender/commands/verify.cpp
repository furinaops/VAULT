#include "vault/object_store.hpp"
#include "vault/sic.hpp"
#include "vault/version_chain.hpp"
#include "vault/name_registry.hpp"
#include <iostream>
#include <filesystem>

namespace fs = std::filesystem;

int cmdVerify(int argc, char* argv[]) {
    std::string until_sic;
    if (argc > 1) until_sic = argv[1];
    std::string home = std::getenv("HOME") ? std::getenv("HOME") : ".";
    std::string storage_path = home + "/.vault/data";
    ObjectStore store(storage_path);
    std::cout << "\n  Verifying chain integrity...\n\n";
    auto versions = store.listVersions();
    bool chain_ok = true;
    for (const auto& v : versions) {
        std::string expected = generate_sic(v.parent_sic, v.tree_hash,
                                              v.author_id, v.timestamp, v.name);
        bool ok = (expected == v.sic);
        if (!ok) chain_ok = false;
        std::cout << "  " << v.short_sic << "  " << v.name << "   "
                  << (ok ? "✓" : "✗");
        if (!ok) std::cout << "  SIC mismatch — history may have been altered";
        std::cout << "\n";
        if (!until_sic.empty() && v.sic == until_sic) break;
    }
    std::cout << "\n  ";
    if (chain_ok) {
        std::cout << "Chain intact. " << versions.size() << " version(s) verified.\n";
    } else {
        std::cout << "Chain integrity check failed — history may have been tampered.\n";
    }
    std::cout << "\n";
    return chain_ok ? 0 : 1;
}
