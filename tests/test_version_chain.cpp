#include "vault/object_store.hpp"
#include "vault/sic.hpp"
#include "vault/version_chain.hpp"
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
    std::string dir = "/tmp/vault_chaintest_" + std::to_string(std::time(nullptr)) + "_" + std::to_string(rand());
    fs::create_directories(dir + "/objects");
    fs::create_directories(dir + "/versions");
    return dir;
}

static void cleanup(const std::string& dir) {
    fs::remove_all(dir);
}

int test_append_and_head() {
    TEST("Append version and get HEAD");
    auto d = makeTestDir();
    ObjectStore store(d);
    VersionChain chain(store);
    Version v;
    v.sic = generate_sic("genesis", "tree1", "aarav", 100, "v1");
    v.short_sic = short_sic(v.sic);
    v.parent_sic = "genesis";
    v.tree_hash = "tree1";
    v.author_id = "aarav";
    v.name = "v1";
    v.timestamp = 100;
    chain.append(v);
    Version head = chain.getHead();
    CHECK(head.sic == v.sic);
    CHECK(head.name == "v1");
    cleanup(d);
    PASS();
    return 0;
}

int test_get_history() {
    TEST("Get version history returns all versions");
    auto d = makeTestDir();
    ObjectStore store(d);
    VersionChain chain(store);
    Version v1;
    v1.sic = generate_sic("genesis", "t1", "a", 100, "first");
    v1.short_sic = short_sic(v1.sic);
    v1.parent_sic = "genesis";
    v1.timestamp = 100;
    v1.name = "first";
    chain.append(v1);
    Version v2;
    v2.sic = generate_sic(v1.sic, "t2", "a", 200, "second");
    v2.short_sic = short_sic(v2.sic);
    v2.parent_sic = v1.sic;
    v2.timestamp = 200;
    v2.name = "second";
    chain.append(v2);
    auto history = chain.getHistory();
    CHECK(history.size() == 2);
    CHECK(history[0].name == "first");
    CHECK(history[1].name == "second");
    cleanup(d);
    PASS();
    return 0;
}

int test_get_since() {
    TEST("Get versions since a given SIC");
    auto d = makeTestDir();
    ObjectStore store(d);
    VersionChain chain(store);
    Version v1;
    v1.sic = generate_sic("genesis", "t1", "a", 100, "v1");
    v1.short_sic = short_sic(v1.sic);
    v1.parent_sic = "genesis";
    v1.timestamp = 100;
    v1.name = "v1";
    chain.append(v1);
    Version v2;
    v2.sic = generate_sic(v1.sic, "t2", "a", 200, "v2");
    v2.short_sic = short_sic(v2.sic);
    v2.parent_sic = v1.sic;
    v2.timestamp = 200;
    v2.name = "v2";
    chain.append(v2);
    Version v3;
    v3.sic = generate_sic(v2.sic, "t3", "a", 300, "v3");
    v3.short_sic = short_sic(v3.sic);
    v3.parent_sic = v2.sic;
    v3.timestamp = 300;
    v3.name = "v3";
    chain.append(v3);
    auto since = chain.getSince(v1.sic);
    CHECK(since.size() == 2);
    CHECK(since[0].name == "v2");
    CHECK(since[1].name == "v3");
    cleanup(d);
    PASS();
    return 0;
}

int test_rollback() {
    TEST("Rollback moves HEAD but keeps history");
    auto d = makeTestDir();
    ObjectStore store(d);
    VersionChain chain(store);
    Version v1;
    v1.sic = generate_sic("genesis", "t1", "a", 100, "v1");
    v1.short_sic = short_sic(v1.sic);
    v1.parent_sic = "genesis";
    v1.timestamp = 100;
    v1.name = "v1";
    chain.append(v1);
    Version v2;
    v2.sic = generate_sic(v1.sic, "t2", "a", 200, "v2");
    v2.short_sic = short_sic(v2.sic);
    v2.parent_sic = v1.sic;
    v2.timestamp = 200;
    v2.name = "v2";
    chain.append(v2);
    CHECK(chain.getHead().name == "v2");
    CHECK(chain.rollback(v1.sic));
    CHECK(chain.getHead().sic == v1.sic);
    auto history = chain.getHistory();
    CHECK(history.size() == 2);
    cleanup(d);
    PASS();
    return 0;
}

int test_verify_integrity() {
    TEST("Verify chain integrity");
    auto d = makeTestDir();
    ObjectStore store(d);
    VersionChain chain(store);
    Version v1;
    v1.sic = generate_sic("genesis", "t1", "a", 100, "v1");
    v1.short_sic = short_sic(v1.sic);
    v1.parent_sic = "genesis";
    v1.tree_hash = "t1";
    v1.author_id = "a";
    v1.timestamp = 100;
    v1.name = "v1";
    chain.append(v1);
    Version v2;
    v2.sic = generate_sic(v1.sic, "t2", "a", 200, "v2");
    v2.short_sic = short_sic(v2.sic);
    v2.parent_sic = v1.sic;
    v2.tree_hash = "t2";
    v2.author_id = "a";
    v2.timestamp = 200;
    v2.name = "v2";
    chain.append(v2);
    CHECK(chain.verifyIntegrity());
    cleanup(d);
    PASS();
    return 0;
}

int main() {
    srand(time(nullptr));
    std::cout << "VersionChain Tests\n\n";
    int result = 0;
    result |= test_append_and_head();
    result |= test_get_history();
    result |= test_get_since();
    result |= test_rollback();
    result |= test_verify_integrity();
    std::cout << "\n" << tests_passed << "/" << tests_run << " tests passed\n";
    return result;
}
