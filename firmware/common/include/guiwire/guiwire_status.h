/*
 * SPDX-License-Identifier: MIT
 * Authors: any:frequency, seige.digital GbR, Leander Seige (2026)
 */

#ifndef GUIWIRE_STATUS_H
#define GUIWIRE_STATUS_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    GUIWIRE_STATUS_OK = 0,
    GUIWIRE_STATUS_ERR_INVALID_ARGUMENT,
    GUIWIRE_STATUS_ERR_BUFFER_TOO_SMALL,
    GUIWIRE_STATUS_ERR_PROTOCOL,
    GUIWIRE_STATUS_ERR_NOT_SUPPORTED,
    GUIWIRE_STATUS_ERR_NOT_FOUND,
    GUIWIRE_STATUS_ERR_ALREADY_EXISTS,
    GUIWIRE_STATUS_ERR_IO,
    GUIWIRE_STATUS_ERR_NO_SPACE,
    GUIWIRE_STATUS_ERR_STATE,
    GUIWIRE_STATUS_ERR_TRANSPORT
} guiwire_status_t;

const char *guiwire_status_str(guiwire_status_t status);

#ifdef __cplusplus
}
#endif

#endif
