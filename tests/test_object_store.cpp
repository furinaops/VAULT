#include "vault/object_store.hpp"
#include "vault/sic.hpp"
#include <iostream>
#include <filesystem>
#include <cassert>
#include <cstdlib>
#include <ctime>

namespace fs = std::filesystem;

static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) do { tests_run++; std::cout << "  " << name << "... "; } while(0)
#define PASS() do { tests_passed++; std::cout << "PASS\n"; } while(0)
#define CHECK(cond) do { if (!(cond)) { std::cout << "FAIL at line " << __LINE__ << "\n"; return 1; } } while(0)

static std::string makeTestDir() {
    std::string dir = "/tmp/vault_ostest_" + std::to_string(std::time(nullptr)) + "_" + std::to_string(rand());
    fs::create_directories(dir + "/objects");
    fs::create_directories(dir + "/versions");
    return dir;
}

static void cleanup(const std::string& dir) {
    fs::remove_all(dir);
}

int test_store_and_load_blob() {
    TEST("Store and load blob");
    auto d = makeTestDir();
    ObjectStore store(d);
    std::vector<uint8_t> data = {'h', 'e', 'l', 'l', 'o'};
    std::string hash = store.storeBlob(data);
    CHECK(!hash.empty());
    CHECK(hash.length() == 64);
    auto loaded = store.loadBlob(hash);
    CHECK(loaded == data);
    CHECK(store.blobExists(hash));
    cleanup(d);
    PASS();
    return 0;
}

int test_blob_dedup() {
    TEST("Blob deduplication (same content returns same hash)");
    auto d = makeTestDir();
    ObjectStore store(d);
    std::vector<uint8_t> data = {'t', 'e', 's', 't'};
    std::string h1 = store.storeBlob(data);
    std::string h2 = store.storeBlob(data);
    CHECK(h1 == h2);
    cleanup(d);
    PASS();
    return 0;
}

int test_store_and_load_tree() {
    TEST("Store and load tree");
    auto d = makeTestDir();
    ObjectStore store(d);
    Tree tree;
    tree.path = "/test";
    tree.entries.push_back({"file1.txt", "abc123", 0644});
    tree.entries.push_back({"file2.txt", "def456", 0755});
    std::string hash = store.storeTree(tree);
    CHECK(!hash.empty());
    CHECK(store.treeExists(hash));
    Tree loaded = store.loadTree(hash);
    CHECK(loaded.entries.size() == 2);
    CHECK(loaded.entries[0].name == "file1.txt");
    CHECK(loaded.entries[1].name == "file2.txt");
    cleanup(d);
    PASS();
    return 0;
}

int test_store_and_load_version() {
    TEST("Store and load version");
    auto d = makeTestDir();
    ObjectStore store(d);
    Version v;
    v.sic = generate_sic("genesis", "treehash", "aarav", 1000, "test");
    v.short_sic = short_sic(v.sic);
    v.parent_sic = "genesis";
    v.tree_hash = "treehash";
    v.author_id = "aarav";
    v.name = "test";
    v.message = "test version";
    v.timestamp = 1000;
    v.changed_files = {"a.cpp", "b.h"};
    store.storeVersion(v);
    CHECK(store.versionExists(v.sic));
    Version loaded = store.loadVersion(v.sic);
    CHECK(loaded.sic == v.sic);
    CHECK(loaded.name == "test");
    CHECK(loaded.author_id == "aarav");
    CHECK(loaded.changed_files.size() == 2);
    cleanup(d);
    PASS();
    return 0;
}

int test_list_versions() {
    TEST("List versions returns chronologically sorted");
    auto d = makeTestDir();
    ObjectStore store(d);
    Version v1;
    v1.sic = generate_sic("genesis", "t1", "a", 100, "first");
    v1.short_sic = short_sic(v1.sic);
    v1.parent_sic = "genesis";
    v1.timestamp = 100;
    v1.name = "first";
    store.storeVersion(v1);
    Version v2;
    v2.sic = generate_sic(v1.sic, "t2", "a", 200, "second");
    v2.short_sic = short_sic(v2.sic);
    v2.parent_sic = v1.sic;
    v2.timestamp = 200;
    v2.name = "second";
    store.storeVersion(v2);
    auto versions = store.listVersions();
    CHECK(versions.size() == 2);
    CHECK(versions[0].name == "first");
    CHECK(versions[1].name == "second");
    cleanup(d);
    PASS();
    return 0;
}

int main() {
    srand(time(nullptr));
    std::cout << "ObjectStore Tests\n\n";
    int result = 0;
    result |= test_store_and_load_blob();
    result |= test_blob_dedup();
    result |= test_store_and_load_tree();
    result |= test_store_and_load_version();
    result |= test_list_versions();
    std::cout << "\n" << tests_passed << "/" << tests_run << " tests passed\n";
    return result;
}
