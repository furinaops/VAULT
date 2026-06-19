#include "vault/sic.hpp"
#include <iostream>
#include <cassert>
#include <cstring>

static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) do { tests_run++; std::cout << "  " << name << "... "; } while(0)
#define PASS() do { tests_passed++; std::cout << "PASS\n"; } while(0)
#define CHECK(cond) do { if (!(cond)) { std::cout << "FAIL at line " << __LINE__ << "\n"; return 1; } } while(0)

int test_sic_generation() {
    TEST("SIC generation is deterministic given same inputs");
    std::string sic1 = generate_sic("genesis", "abc123", "aarav", 1234567890, "initial");
    std::string sic2 = generate_sic("genesis", "abc123", "aarav", 1234567890, "initial");
    CHECK(sic1 == sic2);
    CHECK(sic1.length() == 64);
    PASS();
    return 0;
}

int test_sic_uniqueness() {
    TEST("Different inputs produce different SICs");
    std::string sic1 = generate_sic("genesis", "abc123", "aarav", 1234567890, "v1");
    std::string sic2 = generate_sic("genesis", "abc123", "aarav", 1234567891, "v1");
    std::string sic3 = generate_sic("genesis", "abc123", "aarav", 1234567890, "v2");
    std::string sic4 = generate_sic("abc123", "def456", "aarav", 1234567890, "v1");
    CHECK(sic1 != sic2);
    CHECK(sic1 != sic3);
    CHECK(sic1 != sic4);
    PASS();
    return 0;
}

int test_sic_chaining() {
    TEST("Version 2 SIC depends on version 1 SIC (chaining)");
    std::string sic1 = generate_sic("genesis", "tree1", "aarav", 1000, "first");
    std::string sic2 = generate_sic(sic1, "tree2", "aarav", 2000, "second");
    std::string sic2_wrong = generate_sic("wrong_parent", "tree2", "aarav", 2000, "second");
    CHECK(sic2 != sic2_wrong);
    PASS();
    return 0;
}

int test_sic_verification() {
    TEST("verify_sic returns true for valid SIC");
    std::string sic = generate_sic("genesis", "tree1", "aarav", 1000, "test");
    CHECK(verify_sic(sic, "genesis", "tree1", "aarav", 1000, "test"));
    PASS();
    return 0;
}

int test_sic_verification_fail() {
    TEST("verify_sic returns false for invalid SIC");
    std::string sic = generate_sic("genesis", "tree1", "aarav", 1000, "test");
    CHECK(!verify_sic(sic, "genesis", "tree1", "aarav", 1000, "wrong_name"));
    CHECK(!verify_sic(sic, "wrong", "tree1", "aarav", 1000, "test"));
    CHECK(!verify_sic(sic, "genesis", "wrong_tree", "aarav", 1000, "test"));
    PASS();
    return 0;
}

int test_short_sic() {
    TEST("short_sic returns first 6 characters");
    std::string sic = "abcdef1234567890";
    CHECK(short_sic(sic) == "abcdef");
    CHECK(short_sic("abc") == "abc");
    PASS();
    return 0;
}

int test_is_valid_hex() {
    TEST("is_valid_hex validates hex strings");
    CHECK(is_valid_hex("abcdef0123456789"));
    CHECK(is_valid_hex("ABCDEF"));
    CHECK(!is_valid_hex("xyz"));
    CHECK(!is_valid_hex(""));
    PASS();
    return 0;
}

int test_sha256_hex() {
    TEST("sha256Hex produces 64-char hex string");
    std::string hash = sha256Hex("hello");
    CHECK(hash.length() == 64);
    CHECK(is_valid_hex(hash));
    PASS();
    return 0;
}

int main() {
    std::cout << "SIC Tests\n\n";

    int result = 0;
    result |= test_sic_generation();
    result |= test_sic_uniqueness();
    result |= test_sic_chaining();
    result |= test_sic_verification();
    result |= test_sic_verification_fail();
    result |= test_short_sic();
    result |= test_is_valid_hex();
    result |= test_sha256_hex();

    std::cout << "\n" << tests_passed << "/" << tests_run << " tests passed\n";
    return result;
}
