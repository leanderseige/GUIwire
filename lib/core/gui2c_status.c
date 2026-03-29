#include "gui2c/gui2c_status.h"

const char *gui2c_status_str(gui2c_status_t status) {
    switch (status) {
        case GUI2C_STATUS_OK: return "OK";
        case GUI2C_STATUS_ERR_INVALID_ARGUMENT: return "INVALID_ARGUMENT";
        case GUI2C_STATUS_ERR_BUFFER_TOO_SMALL: return "BUFFER_TOO_SMALL";
        case GUI2C_STATUS_ERR_PROTOCOL: return "PROTOCOL";
        case GUI2C_STATUS_ERR_NOT_SUPPORTED: return "NOT_SUPPORTED";
        case GUI2C_STATUS_ERR_NOT_FOUND: return "NOT_FOUND";
        case GUI2C_STATUS_ERR_ALREADY_EXISTS: return "ALREADY_EXISTS";
        case GUI2C_STATUS_ERR_IO: return "IO";
        case GUI2C_STATUS_ERR_NO_SPACE: return "NO_SPACE";
        case GUI2C_STATUS_ERR_STATE: return "STATE";
        case GUI2C_STATUS_ERR_TRANSPORT: return "TRANSPORT";
        default: return "UNKNOWN";
    }
}
