#include "vault/object_store.hpp"
#include "vault/sic.hpp"
#include "vault/version_chain.hpp"
#include "vault/name_registry.hpp"
#include <iostream>
#include <iomanip>
#include <ctime>
#include <filesystem>

namespace fs = std::filesystem;

int cmdSic(int argc, char* argv[]) {
    bool show_log = false;
    int count = -1;
    std::string target;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--log") show_log = true;
        else if (arg == "--n" && i + 1 < argc) count = std::stoi(argv[++i]);
        else if (arg[0] != '-') target = arg;
    }
    std::string home = std::getenv("HOME") ? std::getenv("HOME") : ".";
    std::string storage_path = home + "/.vault/data";
    ObjectStore store(storage_path);
    NameRegistry names(storage_path);
    if (show_log) {
        auto versions = store.listVersions();
        std::string head_sic = names.getHead();
        std::cout << "\n  ┌──────────────────────────────────────────────────────────────────┐\n";
        std::cout << "  │  VAULT version chain                                             │\n";
        std::cout << "  ├──────────┬─────────┬──────────────────┬──────────────────────────┤\n";
        std::cout << "  │ Name     │ SIC     │ Timestamp        │ Author                   │\n";
        std::cout << "  ├──────────┼─────────┼──────────────────┼──────────────────────────┤\n";
        int shown = 0;
        for (const auto& v : versions) {
            if (count > 0 && shown >= static_cast<int>(versions.size()) - count) break;
            auto t = static_cast<time_t>(v.timestamp);
            std::tm tm;
            localtime_r(&t, &tm);
            std::cout << "  │ " << std::left << std::setw(8) << v.name
                      << " │ " << std::setw(7) << v.short_sic
                      << " │ " << std::put_time(&tm, "%Y-%m-%d %H:%M")
                      << " │ " << std::setw(24) << v.author_id;
            if (v.sic == head_sic) std::cout << " HEAD";
            std::cout << " │\n";
            shown++;
            if (count > 0 && shown >= count) break;
        }
        std::cout << "  └──────────┴─────────┴──────────────────┴──────────────────────────┘\n";
        return 0;
    }
    if (target.empty()) target = "latest";
    std::string full_sic = names.resolve(target);
    if (full_sic.empty()) {
        full_sic = target;
    }
    if (!store.versionExists(full_sic) && full_sic.length() < 64) {
        std::cerr << "✗ Version not found: \"" << target << "\"\n";
        return 1;
    }
    if (store.versionExists(full_sic)) {
        Version v = store.loadVersion(full_sic);
        std::cout << "  SIC: " << v.sic << "\n";
        std::cout << "  Short: " << v.short_sic << "\n";
        std::cout << "  Name: " << v.name << "\n";
        std::cout << "  Parent: " << v.parent_sic << "\n";
        std::cout << "  Author: " << v.author_id << "\n";
        auto t = static_cast<time_t>(v.timestamp);
        std::cout << "  Time: " << std::put_time(std::localtime(&t), "%Y-%m-%d %H:%M:%S") << "\n";
        if (!v.message.empty()) std::cout << "  Message: " << v.message << "\n";
        return 0;
    }
    std::cerr << "✗ Version not found: \"" << target << "\"\n";
    return 1;
}
