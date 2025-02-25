/*
 * infra_error.c - Error handling implementation
 */

#include "internal/infra/infra_core.h"
#include "internal/infra/infra_error.h"

// 预期错误状态
static struct {
    bool has_expected;
    infra_error_t expected_error;
} g_error_state = {false, INFRA_OK};

const char* infra_error_string(infra_error_t err) {
    switch (err) {
        case INFRA_OK:
            return "Success";
        case INFRA_ERROR_UNKNOWN:
            return "Unknown error";
        case INFRA_ERROR_INVALID:
            return "Invalid parameter";
        case INFRA_ERROR_INVALID_PARAM:
            return "Invalid parameter";
        case INFRA_ERROR_NO_MEMORY:
            return "No memory";
        case INFRA_ERROR_EXISTS:
            return "Already exists";
        case INFRA_ERROR_NOT_READY:
            return "Not ready";
        case INFRA_ERROR_IO:
            return "I/O error";
        case INFRA_ERROR_TIMEOUT:
            return "Timeout";
        case INFRA_ERROR_BUSY:
            return "Resource busy";
        case INFRA_ERROR_DEPENDENCY:
            return "Dependency error";
        case INFRA_ERROR_NOT_FOUND:
            return "Not found";
        case INFRA_ERROR_SYSTEM:
            return "System error";
        case INFRA_ERROR_WOULD_BLOCK:
            return "Operation would block";
        case INFRA_ERROR_CLOSED:
            return "Resource closed";
        case INFRA_ERROR_NOT_SUPPORTED:
            return "Not supported";
        case INFRA_ERROR_ALREADY_EXISTS:
            return "Already exists";
        case INFRA_ERROR_INVALID_OPERATION:
            return "Invalid operation";
        case INFRA_ERROR_RUNTIME:
            return "Runtime error";
        case INFRA_ERROR_INVALID_STATE:
            return "Invalid state";
        case INFRA_ERROR_INVALID_CONFIG:
            return "Invalid configuration";
        case INFRA_ERROR_CAS_MISMATCH:
            return "CAS mismatch";
        case INFRA_ERROR_INVALID_TYPE:
            return "Invalid type";
        case INFRA_ERROR_PROTOCOL:
            return "Protocol error";
        case INFRA_ERROR_CONNECT_FAILED:
            return "Connection failed";
        default:
            return "Unknown error";
    }
}

void infra_set_expected_error(infra_error_t err) {
    g_error_state.has_expected = true;
    g_error_state.expected_error = err;
}

void infra_clear_expected_error(void) {
    g_error_state.has_expected = false;
    g_error_state.expected_error = INFRA_OK;
}

bool infra_is_expected_error(infra_error_t err) {
    return g_error_state.has_expected && g_error_state.expected_error == err;
} 
