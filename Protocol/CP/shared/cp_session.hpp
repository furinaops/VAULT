#pragma once
#include "cp_types.hpp"
#include <cstdint>
#include <string>

class CPSession {
public:
    SessionState state = SessionState::IDLE;
    uint32_t session_id = 0;
    uint32_t next_turn = 1;       // next outgoing turn id (dialogue only)
    uint32_t last_seen_turn = 0;  // last received turn id

    std::string stored_trigger_word;
    std::string stored_trigger_params;
    AbortCause abort_cause = AbortCause::TIMEOUT;

    void reset(uint32_t new_session_id);
    bool processIncoming(DialogueRole role);
    void abort(AbortCause cause);
};
