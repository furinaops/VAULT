#include "vault/delta.hpp"
#include "vault/sic.hpp"
#include <iostream>
#include <filesystem>
#include <cassert>
#include <fstream>
#include <cstdlib>
#include <ctime>

namespace fs = std::filesystem;

static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) do { tests_run++; std::cout << "  " << name << "... "; } while(0)
#define PASS() do { tests_passed++; std::cout << "PASS\n"; } while(0)
#define CHECK(cond) do { if (!(cond)) { std::cout << "FAIL at line " << __LINE__ << "\n"; return 1; } } while(0)

static std::string makeTestDir() {
    std::string dir = "/tmp/vault_deltatest_" + std::to_string(std::time(nullptr)) + "_" + std::to_string(rand());
    fs::create_directories(dir + "/src");
    return dir;
}

static void cleanup(const std::string& dir) {
    fs::remove_all(dir);
}

int test_empty_delta() {
    TEST("Empty directory produces no delta against empty tree");
    auto d = makeTestDir();
    VaultIgnore ignorer;
    ignorer.setDefaults();
    Tree empty_tree;
    DeltaResult delta = compute_delta(d, empty_tree, ignorer);
    CHECK(delta.files.empty());
    cleanup(d);
    PASS();
    return 0;
}

int test_file_addition() {
    TEST("Adding a file produces ADDED delta");
    auto d = makeTestDir();
    std::ofstream(d + "/newfile.txt") << "hello world\n";
    VaultIgnore ignorer;
    ignorer.setDefaults();
    Tree empty_tree;
    DeltaResult delta = compute_delta(d, empty_tree, ignorer);
    CHECK(delta.files.size() == 1);
    CHECK(delta.files[0].operation == FileDelta::Op::ADDED);
    CHECK(delta.files[0].path == "newfile.txt");
    cleanup(d);
    PASS();
    return 0;
}

int test_file_modification() {
    TEST("Modifying a file produces MODIFIED delta");
    auto d = makeTestDir();
    std::ofstream(d + "/file.txt") << "original\n";
    VaultIgnore ignorer;
    ignorer.setDefaults();
    Tree empty_tree;
    Tree prev_tree = buildTreeFromPath(d, ignorer);
    std::ofstream(d + "/file.txt") << "modified\n";
    DeltaResult delta = compute_delta(d, prev_tree, ignorer);
    CHECK(delta.files.size() == 1);
    CHECK(delta.files[0].operation == FileDelta::Op::MODIFIED);
    CHECK(delta.files[0].path == "file.txt");
    cleanup(d);
    PASS();
    return 0;
}

int test_file_deletion() {
    TEST("Deleting a file produces DELETED delta");
    auto d = makeTestDir();
    std::string filepath = d + "/todelete.txt";
    std::ofstream(filepath) << "delete me\n";
    VaultIgnore ignorer;
    ignorer.setDefaults();
    Tree prev_tree = buildTreeFromPath(d, ignorer);
    fs::remove(filepath);
    DeltaResult delta = compute_delta(d, prev_tree, ignorer);
    CHECK(delta.files.size() == 1);
    CHECK(delta.files[0].operation == FileDelta::Op::DELETED);
    CHECK(delta.files[0].path == "todelete.txt");
    cleanup(d);
    PASS();
    return 0;
}

int test_vaultignore() {
    TEST("Files matching .vaultignore are excluded");
    auto d = makeTestDir();
    std::ofstream(d + "/secret.key") << "sensitive\n";
    std::ofstream(d + "/normal.cpp") << "code\n";
    std::ofstream(d + "/.vaultignore") << "*.key\n";
    VaultIgnore ignorer(d + "/.vaultignore");
    Tree empty_tree;
    DeltaResult delta = compute_delta(d, empty_tree, ignorer);
    bool has_secret = false;
    bool has_normal = false;
    for (const auto& d : delta.files) {
        if (d.path.find("secret.key") != std::string::npos) has_secret = true;
        if (d.path == "normal.cpp") has_normal = true;
    }
    CHECK(!has_secret);
    CHECK(has_normal);
    cleanup(d);
    PASS();
    return 0;
}

int test_pack_unpack_payload() {
    TEST("Pack and unpack payload roundtrip");
    std::vector<FileDelta> deltas;
    FileDelta d1;
    d1.operation = FileDelta::Op::ADDED;
    d1.path = "test.txt";
    d1.blob_hash = "abc123";
    d1.content = {'h', 'e', 'l', 'l', 'o'};
    deltas.push_back(d1);
    Version v;
    v.sic = generate_sic("genesis", "tree1", "aarav", 1000, "test");
    v.short_sic = short_sic(v.sic);
    v.parent_sic = "genesis";
    v.tree_hash = "tree1";
    v.author_id = "aarav";
    v.name = "test";
    v.message = "test msg";
    v.timestamp = 1000;
    v.changed_files = {"test.txt"};
    auto packed = pack_payload(deltas, v);
    auto [unpacked_v, unpacked_deltas] = unpack_payload(packed);
    CHECK(unpacked_v.sic == v.sic);
    CHECK(unpacked_v.name == "test");
    CHECK(unpacked_deltas.size() == 1);
    CHECK(unpacked_deltas[0].path == "test.txt");
    CHECK(unpacked_deltas[0].content.size() == 5);
    PASS();
    return 0;
}

int main() {
    srand(time(nullptr));
    std::cout << "Delta Tests\n\n";
    int result = 0;
    result |= test_empty_delta();
    result |= test_file_addition();
    result |= test_file_modification();
    result |= test_file_deletion();
    result |= test_vaultignore();
    result |= test_pack_unpack_payload();
    std::cout << "\n" << tests_passed << "/" << tests_run << " tests passed\n";
    return result;
}
