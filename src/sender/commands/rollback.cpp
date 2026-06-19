#include "vault/object_store.hpp"
#include "vault/sic.hpp"
#include "vault/version_chain.hpp"
#include "vault/name_registry.hpp"
#include <iostream>
#include <fstream>
#include <filesystem>

namespace fs = std::filesystem;

int cmdRollback(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: /ROLLBACK \"<sic_or_name>\"\n";
        return 1;
    }
    std::string target = argv[1];
    std::string home = std::getenv("HOME") ? std::getenv("HOME") : ".";
    std::string storage_path = home + "/.vault/data";
    ObjectStore store(storage_path);
    VersionChain chain(store);
    NameRegistry names(storage_path);
    std::string full_sic = names.resolve(target);
    if (full_sic.empty() || !store.versionExists(full_sic)) {
        std::cerr << "✗ Version not found: \"" << target << "\"\n";
        return 1;
    }
    Version v = store.loadVersion(full_sic);
    std::cout << "  ⚠ Rolling back to " << v.name << " (" << v.short_sic << ")...\n";
    chain.setHead(full_sic);
    names.setHead(full_sic);
    {
        std::string head_path = home + "/.vault/HEAD";
        std::ofstream f(head_path);
        f << full_sic << std::endl;
    }
    std::cout << "  ✓ HEAD is now " << v.name << " (" << v.short_sic << ")\n";
    std::cout << "  Newer versions still exist. Use /SIC --log to see full chain.\n";
    return 0;
}
