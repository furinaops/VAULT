#pragma once
#include "cp_types.hpp"
#include <cstdint>
#include <vector>
#include <string>
#include <array>

struct CPPacket {
    DialogueRole role = DialogueRole::HEARTBEAT;
    uint32_t session_id = 0;
    uint32_t turn_id = 0;
    uint16_t agent_id = 0;
    std::array<char, 16> trigger{};   // null-padded ASCII
    std::vector<uint8_t> payload;

    std::vector<uint8_t> serialize() const;
    static CPPacket deserialize(const uint8_t* buf, size_t len);
    static uint32_t computeCRC32(const uint8_t* data, size_t len);
    std::string getTriggerWord() const;
};
