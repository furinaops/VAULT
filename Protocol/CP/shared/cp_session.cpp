#include "cp_session.hpp"

void CPSession::reset(uint32_t new_session_id) {
    state = SessionState::IDLE;
    session_id = new_session_id;
    next_turn = 1;
    last_seen_turn = 0;
    stored_trigger_word.clear();
    stored_trigger_params.clear();
}

bool CPSession::processIncoming(DialogueRole role) {
    switch (role) {
    case DialogueRole::PROPOSE:
        if (state == SessionState::IDLE || state == SessionState::CONCLUDED || state == SessionState::ABORTED) {
            state = SessionState::PROPOSED;
            return true;
        }
        break;
    case DialogueRole::ACCEPT:
        if (state == SessionState::PROPOSED) { state = SessionState::DELIBERATING; return true; }
        if (state == SessionState::DELIBERATING) { state = SessionState::COMMITTED; return true; }
        break;
    case DialogueRole::COUNTER:
        if (state == SessionState::PROPOSED || state == SessionState::DELIBERATING) { state = SessionState::DELIBERATING; return true; }
        break;
    case DialogueRole::CLARIFY:
        if (state == SessionState::PROPOSED || state == SessionState::DELIBERATING) { state = SessionState::DELIBERATING; return true; }
        break;
    case DialogueRole::INFORM:
        if (state == SessionState::DELIBERATING) return true;
        break;
    case DialogueRole::REJECT:
        if (state == SessionState::PROPOSED || state == SessionState::DELIBERATING) { state = SessionState::ABORTED; return true; }
        break;
    case DialogueRole::ABORT:
        if (state == SessionState::PROPOSED || state == SessionState::DELIBERATING ||
            state == SessionState::COMMITTED || state == SessionState::EXECUTING) {
            state = SessionState::ABORTED; return true;
        }
        break;
    case DialogueRole::COMMIT:
        if (state == SessionState::DELIBERATING) { state = SessionState::EXECUTING; return true; }
        break;
    case DialogueRole::PROGRESS:
        if (state == SessionState::EXECUTING) return true;
        break;
    case DialogueRole::CONCLUDE:
        if (state == SessionState::EXECUTING) { state = SessionState::CONCLUDED; return true; }
        break;
    case DialogueRole::ROLLBACK:
        if (state == SessionState::EXECUTING) { state = SessionState::ABORTED; return true; }
        break;
    default: ;
    }
    return false;
}

void CPSession::abort(AbortCause cause) {
    state = SessionState::ABORTED;
    abort_cause = cause;
}
