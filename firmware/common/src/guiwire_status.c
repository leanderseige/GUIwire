/*
 * SPDX-License-Identifier: MIT
 * Authors: any:frequency, seige.digital GbR, Leander Seige (2026)
 */

#include "guiwire/guiwire_status.h"

const char *guiwire_status_str(guiwire_status_t status) {
    switch (status) {
        case GUIWIRE_STATUS_OK: return "OK";
        case GUIWIRE_STATUS_ERR_INVALID_ARGUMENT: return "INVALID_ARGUMENT";
        case GUIWIRE_STATUS_ERR_BUFFER_TOO_SMALL: return "BUFFER_TOO_SMALL";
        case GUIWIRE_STATUS_ERR_PROTOCOL: return "PROTOCOL";
        case GUIWIRE_STATUS_ERR_NOT_SUPPORTED: return "NOT_SUPPORTED";
        case GUIWIRE_STATUS_ERR_NOT_FOUND: return "NOT_FOUND";
        case GUIWIRE_STATUS_ERR_ALREADY_EXISTS: return "ALREADY_EXISTS";
        case GUIWIRE_STATUS_ERR_IO: return "IO";
        case GUIWIRE_STATUS_ERR_NO_SPACE: return "NO_SPACE";
        case GUIWIRE_STATUS_ERR_STATE: return "STATE";
        case GUIWIRE_STATUS_ERR_TRANSPORT: return "TRANSPORT";
        default: return "UNKNOWN";
    }
}
