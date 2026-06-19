#pragma once
#include <cstdint>

enum class DialogueRole : uint8_t {
    AUTH         = 0x00,
    AUTH_OK      = 0x01,
    AUTH_FAIL    = 0x02,
    PROPOSE      = 0x03,
    COUNTER      = 0x04,
    CLARIFY      = 0x05,
    INFORM       = 0x06,
    ACCEPT       = 0x07,
    REJECT       = 0x08,
    COMMIT       = 0x09,
    PROGRESS     = 0x0A,
    CONCLUDE     = 0x0B,
    ABORT        = 0x0C,
    ROLLBACK     = 0x0D,
    HEARTBEAT    = 0x0E,
    KEY_ECHO     = 0x0F,
    BARTER_WARN  = 0x10,
    SESSION_INIT = 0x11,
};

enum class SessionState : uint8_t {
    IDLE,
    PROPOSED,
    DELIBERATING,
    COMMITTED,
    EXECUTING,
    CONCLUDED,
    ABORTED,
};

enum class AbortCause : uint8_t {
    TIMEOUT            = 0x01,
    KEY_MISMATCH       = 0x02,
    ECHO_TIMEOUT       = 0x03,
    INVALID_TRANSITION = 0x04,
    UNKNOWN_TRIGGER    = 0x05,
    TRIGGER_FAILED     = 0x06,
    OBSTACLE_DETECTED  = 0x07,
    RESOURCE_BUSY      = 0x08,
    TURN_MISMATCH      = 0x09,
    AUTH_FAILED        = 0x0A,
    LIVENESS_TIMEOUT   = 0x0B,
};
