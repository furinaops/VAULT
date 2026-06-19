#pragma once
#include <array>
#include <cstdint>
#include <string>

namespace CPCrypto {
    std::array<uint8_t, 32> generateLoginKey();
    std::array<uint8_t, 32> generateSessionKey(uint16_t agent_id, uint32_t session_id);
    std::array<uint8_t, 32> computeEchoToken(const std::array<uint8_t, 32>& session_key,
                                             uint32_t turn_id);
    std::string bytesToHex(const std::array<uint8_t, 32>& data);
    std::array<uint8_t, 32> hexToBytes(const std::string& hex);
}
