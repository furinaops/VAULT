#include "vault/sic.hpp"
#include <openssl/sha.h>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <algorithm>

std::string sha256Hex(const std::string& input) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(input.data()), input.size(), hash);
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i)
        oss << std::setw(2) << static_cast<int>(hash[i]);
    return oss.str();
}

std::string generate_sic(
    const std::string& parent_sic,
    const std::string& tree_hash,
    const std::string& author_id,
    int64_t timestamp,
    const std::string& name)
{
    std::ostringstream oss;
    oss << parent_sic << '\0'
        << tree_hash << '\0'
        << author_id << '\0'
        << timestamp << '\0'
        << name;
    return sha256Hex(oss.str());
}

std::string short_sic(const std::string& full_sic) {
    if (full_sic.length() < 6) return full_sic;
    return full_sic.substr(0, 6);
}

bool verify_sic(
    const std::string& sic,
    const std::string& parent_sic,
    const std::string& tree_hash,
    const std::string& author_id,
    int64_t timestamp,
    const std::string& name)
{
    std::string computed = generate_sic(parent_sic, tree_hash, author_id, timestamp, name);
    return sic == computed;
}

bool is_valid_hex(const std::string& s) {
    if (s.empty()) return false;
    return std::all_of(s.begin(), s.end(), [](char c) {
        return (c >= '0' && c <= '9') ||
               (c >= 'a' && c <= 'f') ||
               (c >= 'A' && c <= 'F');
    });
}

bool is_short_sic(const std::string& s) {
    return s.length() >= 6 && s.length() < 64 && is_valid_hex(s);
}
