#include "vault/object_store.hpp"
#include "vault/sic.hpp"
#include "vault/version_chain.hpp"
#include "vault/name_registry.hpp"
#include <iostream>
#include <filesystem>
#include <sstream>
#include <algorithm>
#include <set>

namespace fs = std::filesystem;

static std::vector<std::string> splitLines(const std::string& s) {
    std::vector<std::string> lines;
    std::istringstream ss(s);
    std::string line;
    while (std::getline(ss, line))
        lines.push_back(line);
    return lines;
}

int cmdDiff(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: /DIFF \"<sic_or_name_1>\" \"<sic_or_name_2>\"\n";
        return 1;
    }
    std::string home = std::getenv("HOME") ? std::getenv("HOME") : ".";
    std::string storage_path = home + "/.vault/data";
    ObjectStore store(storage_path);
    NameRegistry names(storage_path);
    std::string sic1 = names.resolve(argv[1]);
    std::string sic2 = names.resolve(argv[2]);
    if (sic1.empty() || !store.versionExists(sic1)) {
        std::cerr << "✗ Version not found: \"" << argv[1] << "\"\n";
        return 1;
    }
    if (sic2.empty() || !store.versionExists(sic2)) {
        std::cerr << "✗ Version not found: \"" << argv[2] << "\"\n";
        return 1;
    }
    Version v1 = store.loadVersion(sic1);
    Version v2 = store.loadVersion(sic2);
    Tree t1 = store.loadTree(v1.tree_hash);
    Tree t2 = store.loadTree(v2.tree_hash);
    std::cout << "\n  Comparing " << v1.name << " (" << v1.short_sic
              << ") → " << v2.name << " (" << v2.short_sic << ")\n\n";
    std::unordered_map<std::string, std::string> map1, map2;
    for (const auto& e : t1.entries) map1[e.name] = e.blob_hash;
    for (const auto& e : t2.entries) map2[e.name] = e.blob_hash;
    std::set<std::string> all_files;
    for (const auto& [name, _] : map1) all_files.insert(name);
    for (const auto& [name, _] : map2) all_files.insert(name);
    bool any_changes = false;
    for (const auto& fname : all_files) {
        auto it1 = map1.find(fname);
        auto it2 = map2.find(fname);
        if (it1 == map1.end()) {
            std::cout << "  " << fname << " [added]\n";
            auto content = store.loadBlob(it2->second);
            auto lines = splitLines(std::string(content.begin(), content.end()));
            for (size_t i = 0; i < lines.size(); ++i)
                std::cout << "    + " << lines[i] << "\n";
            any_changes = true;
        } else if (it2 == map2.end()) {
            std::cout << "  " << fname << " [deleted]\n";
            any_changes = true;
        } else if (it1->second != it2->second) {
            std::cout << "  " << fname << "\n";
            auto old_content = store.loadBlob(it1->second);
            auto new_content = store.loadBlob(it2->second);
            auto old_lines = splitLines(std::string(old_content.begin(), old_content.end()));
            auto new_lines = splitLines(std::string(new_content.begin(), new_content.end()));
            size_t max_lines = std::max(old_lines.size(), new_lines.size());
            for (size_t i = 0; i < max_lines; ++i) {
                if (i >= old_lines.size()) {
                    std::cout << "    + line " << (i + 1) << ": " << new_lines[i] << "\n";
                } else if (i >= new_lines.size()) {
                    std::cout << "    - line " << (i + 1) << ": " << old_lines[i] << "\n";
                } else if (old_lines[i] != new_lines[i]) {
                    std::cout << "    - line " << (i + 1) << ": " << old_lines[i] << "\n";
                    std::cout << "    + line " << (i + 1) << ": " << new_lines[i] << "\n";
                }
            }
            any_changes = true;
        }
    }
    if (!any_changes)
        std::cout << "  No differences found.\n";
    std::cout << "\n";
    return 0;
}
