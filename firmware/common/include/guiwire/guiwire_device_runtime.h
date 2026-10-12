/*
 * SPDX-License-Identifier: MIT
 * Authors: any:frequency, seige.digital GbR, Leander Seige (2026)
 */

#ifndef GUIWIRE_DEVICE_RUNTIME_H
#define GUIWIRE_DEVICE_RUNTIME_H

#include <stdbool.h>
#include <stdint.h>

#include "guiwire/guiwire_device.h"
#include "guiwire/guiwire_link.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*guiwire_device_tx_complete_fn)(void *ctx);
typedef void (*guiwire_device_tx_begin_fn)(void *ctx);
typedef void (*guiwire_device_session_fn)(void *ctx, guiwire_device_t *device);
typedef guiwire_status_t (*guiwire_device_set_baud_fn)(void *ctx, uint32_t baud);

typedef struct {
    guiwire_link_write_fn write;
    void *write_ctx;
    guiwire_device_tx_begin_fn tx_begin;
    void *tx_begin_ctx;
    guiwire_device_tx_complete_fn tx_complete;
    void *tx_complete_ctx;
    uint32_t session_timeout_ms;
    uint32_t echo_guard_ms;
    /* Optional rate negotiation. The Device always falls back to base_baud. */
    uint32_t base_baud;
    const uint32_t *supported_bauds;
    uint8_t supported_baud_count;
    uint32_t baud_fallback_ms;
    guiwire_device_set_baud_fn set_baud;
    void *set_baud_ctx;
} guiwire_device_io_t;

typedef struct {
    guiwire_device_t device;
    guiwire_link_t link;
    guiwire_device_io_t io;
    guiwire_device_session_fn on_session_start;
    guiwire_device_session_fn on_session_end;
    void *session_ctx;
    uint32_t now_ms;
    uint32_t last_host_ms;
    uint32_t tx_guard_until_ms;
    uint32_t current_baud;
    uint32_t baud_fallback_deadline_ms;
    bool baud_fallback_active;
    bool reliable_session;
    bool inflight;
    uint8_t session_token[GUIWIRE_SESSION_TOKEN_SIZE];
    uint16_t next_sequence;
} guiwire_device_runtime_t;

guiwire_status_t guiwire_device_runtime_init(guiwire_device_runtime_t *runtime,
                                             const guiwire_device_io_t *io,
                                             guiwire_device_session_fn on_session_start,
                                             guiwire_device_session_fn on_session_end,
                                             void *session_ctx);

void guiwire_device_runtime_feed_byte(guiwire_device_runtime_t *runtime,
                                      uint8_t byte,
                                      uint32_t now_ms);
void guiwire_device_runtime_tick(guiwire_device_runtime_t *runtime, uint32_t now_ms);

guiwire_device_t *guiwire_device_runtime_device(guiwire_device_runtime_t *runtime);
bool guiwire_device_runtime_connected(const guiwire_device_runtime_t *runtime);

#ifdef __cplusplus
}
#endif

#endif
