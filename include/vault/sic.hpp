#pragma once
#include <string>
#include <cstdint>

std::string sha256Hex(const std::string& input);

std::string generate_sic(
    const std::string& parent_sic,
    const std::string& tree_hash,
    const std::string& author_id,
    int64_t timestamp,
    const std::string& name
);

std::string short_sic(const std::string& full_sic);

bool verify_sic(
    const std::string& sic,
    const std::string& parent_sic,
    const std::string& tree_hash,
    const std::string& author_id,
    int64_t timestamp,
    const std::string& name
);

bool is_valid_hex(const std::string& s);

bool is_short_sic(const std::string& s);
