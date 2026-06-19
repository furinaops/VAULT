#include "cp_crypto.hpp"
#include <openssl/sha.h>
#include <openssl/hmac.h>
#include <random>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <stdexcept>

std::array<uint8_t, 32> CPCrypto::generateLoginKey() {
    std::random_device rd;
    std::array<uint8_t, 32> rand_input;
    for (auto& b : rand_input) b = static_cast<uint8_t>(rd());

    std::array<uint8_t, 32> result;
    SHA256(rand_input.data(), rand_input.size(), result.data());
    return result;
}

std::array<uint8_t, 32> CPCrypto::generateSessionKey(uint16_t agent_id, uint32_t session_id) {
    std::array<uint8_t, 18> input;
    // agent_id (big-endian)
    input[0] = static_cast<uint8_t>(agent_id >> 8);
    input[1] = static_cast<uint8_t>(agent_id & 0xFF);
    // session_id (big-endian)
    input[2] = static_cast<uint8_t>(session_id >> 24);
    input[3] = static_cast<uint8_t>((session_id >> 16) & 0xFF);
    input[4] = static_cast<uint8_t>((session_id >> 8) & 0xFF);
    input[5] = static_cast<uint8_t>(session_id & 0xFF);

    // random 4 bytes
    std::random_device rd;
    uint32_t rnd = rd();
    input[6] = static_cast<uint8_t>(rnd >> 24);
    input[7] = static_cast<uint8_t>((rnd >> 16) & 0xFF);
    input[8] = static_cast<uint8_t>((rnd >> 8) & 0xFF);
    input[9] = static_cast<uint8_t>(rnd & 0xFF);

    // steady_clock timestamp (8 bytes)
    auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    for (int i = 0; i < 8; ++i)
        input[10 + i] = static_cast<uint8_t>((now >> (56 - i * 8)) & 0xFF);

    std::array<uint8_t, 32> result;
    SHA256(input.data(), input.size(), result.data());
    return result;
}

std::array<uint8_t, 32> CPCrypto::computeEchoToken(const std::array<uint8_t, 32>& session_key,
                                                   uint32_t turn_id) {
    // turn_id as little-endian
    std::array<uint8_t, 4> turn_le;
    turn_le[0] = turn_id & 0xFF;
    turn_le[1] = (turn_id >> 8) & 0xFF;
    turn_le[2] = (turn_id >> 16) & 0xFF;
    turn_le[3] = (turn_id >> 24) & 0xFF;

    std::array<uint8_t, 32> mac;
    unsigned int outlen = 32;
    HMAC(EVP_sha256(), session_key.data(), session_key.size(),
         turn_le.data(), turn_le.size(), mac.data(), &outlen);
    return mac;
}

std::string CPCrypto::bytesToHex(const std::array<uint8_t, 32>& data) {
    std::ostringstream oss;
    oss << std::hex << std::uppercase << std::setfill('0');
    for (auto b : data) oss << std::setw(2) << static_cast<int>(b);
    return oss.str();
}

std::array<uint8_t, 32> CPCrypto::hexToBytes(const std::string& hex) {
    if (hex.length() != 64) throw std::runtime_error("Invalid hex length");
    std::array<uint8_t, 32> arr;
    for (size_t i = 0; i < 32; ++i)
        arr[i] = static_cast<uint8_t>(std::stoul(hex.substr(i*2, 2), nullptr, 16));
    return arr;
}
