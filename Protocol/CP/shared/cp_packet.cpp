#include "cp_packet.hpp"
#include <cstring>
#include <stdexcept>
#include <arpa/inet.h>

namespace {
    uint32_t crc32_table[256];
    bool crc_initialized = false;
    void init_crc32() {
        if (crc_initialized) return;
        for (uint32_t i = 0; i < 256; ++i) {
            uint32_t crc = i;
            for (int j = 0; j < 8; ++j)
                crc = (crc >> 1) ^ (crc & 1 ? 0xEDB88320 : 0);
            crc32_table[i] = crc;
        }
        crc_initialized = true;
    }
}

uint32_t CPPacket::computeCRC32(const uint8_t* data, size_t len) {
    init_crc32();
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < len; ++i)
        crc = (crc >> 8) ^ crc32_table[(crc ^ data[i]) & 0xFF];
    return ~crc;
}

std::vector<uint8_t> CPPacket::serialize() const {
    std::vector<uint8_t> buf;
    buf.push_back('C');
    buf.push_back('P');
    buf.push_back(0x01);                         // version
    buf.push_back(static_cast<uint8_t>(role));

    uint32_t n_sid = htonl(session_id);
    buf.insert(buf.end(), reinterpret_cast<const uint8_t*>(&n_sid),
               reinterpret_cast<const uint8_t*>(&n_sid) + 4);
    uint32_t n_turn = htonl(turn_id);
    buf.insert(buf.end(), reinterpret_cast<const uint8_t*>(&n_turn),
               reinterpret_cast<const uint8_t*>(&n_turn) + 4);
    uint16_t n_agent = htons(agent_id);
    buf.insert(buf.end(), reinterpret_cast<const uint8_t*>(&n_agent),
               reinterpret_cast<const uint8_t*>(&n_agent) + 2);

    for (size_t i = 0; i < 16; ++i)
        buf.push_back(static_cast<uint8_t>(trigger[i]));

    uint32_t plen = htonl(static_cast<uint32_t>(payload.size()));
    buf.insert(buf.end(), reinterpret_cast<const uint8_t*>(&plen),
               reinterpret_cast<const uint8_t*>(&plen) + 4);
    buf.insert(buf.end(), payload.begin(), payload.end());

    uint32_t crc = computeCRC32(buf.data(), buf.size());
    uint32_t n_crc = htonl(crc);
    buf.insert(buf.end(), reinterpret_cast<const uint8_t*>(&n_crc),
               reinterpret_cast<const uint8_t*>(&n_crc) + 4);
    return buf;
}

CPPacket CPPacket::deserialize(const uint8_t* buf, size_t len) {
    if (len < 34) throw std::runtime_error("Packet too short");
    if (buf[0] != 'C' || buf[1] != 'P') throw std::runtime_error("Invalid magic");
    if (buf[2] != 0x01) throw std::runtime_error("Unsupported version");

    CPPacket p;
    p.role = static_cast<DialogueRole>(buf[3]);

    uint32_t n_sid;
    std::memcpy(&n_sid, buf + 4, 4);
    p.session_id = ntohl(n_sid);

    uint32_t n_turn;
    std::memcpy(&n_turn, buf + 8, 4);
    p.turn_id = ntohl(n_turn);

    uint16_t n_agent;
    std::memcpy(&n_agent, buf + 12, 2);
    p.agent_id = ntohs(n_agent);

    std::memcpy(p.trigger.data(), buf + 14, 16);

    uint32_t plen;
    std::memcpy(&plen, buf + 30, 4);
    plen = ntohl(plen);
    if (34 + plen + 4 > len) throw std::runtime_error("Payload length exceeds buffer");

    p.payload.assign(buf + 34, buf + 34 + plen);
    return p;
}

std::string CPPacket::getTriggerWord() const {
    const char* t = trigger.data();
    return std::string(t, strnlen(t, 16));
}
