/*
 * SPDX-License-Identifier: MIT
 * Authors: any:frequency, seige.digital GbR, Leander Seige (2026)
 */

#ifndef GUIWIRE_HOST_H
#define GUIWIRE_HOST_H

#include <stdbool.h>
#include <stdint.h>

#include "guiwire/guiwire_link.h"
#include "guiwire/guiwire_server.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*guiwire_host_tx_complete_fn)(void *ctx);
typedef guiwire_status_t (*guiwire_host_set_baud_fn)(void *ctx, uint32_t baud);
typedef void (*guiwire_host_session_token_fn)(void *ctx,
                                              uint8_t token[GUIWIRE_SESSION_TOKEN_SIZE]);
typedef uint32_t (*guiwire_host_time_ms_fn)(void *ctx);

typedef struct {
    guiwire_link_write_fn write;
    void *write_ctx;
    guiwire_host_tx_complete_fn tx_complete;
    void *tx_complete_ctx;
    uint32_t response_timeout_ms;
    uint32_t discovery_interval_ms;
    uint32_t poll_interval_ms;
    uint32_t session_timeout_ms;
    uint8_t max_missed_responses;
    /* Optional rate negotiation. base_baud is the rate used before discovery
     * and after recovery. */
    uint32_t base_baud;
    const uint32_t *supported_bauds;
    uint8_t supported_baud_count;
    uint32_t baud_guard_ms;
    /* Must match the Device's silent high-rate fallback interval. */
    uint32_t baud_fallback_ms;
    guiwire_host_set_baud_fn set_baud;
    void *set_baud_ctx;
    /* Supply a fresh unpredictable token for each PING. When required, peers
     * without session-aware replies are not accepted as connected. */
    guiwire_host_session_token_fn new_session_token;
    void *session_token_ctx;
    bool require_reliable_session;
    /* Optional monotonic clock: refresh deadlines after synchronous drawing. */
    guiwire_host_time_ms_fn read_time_ms;
    void *time_ctx;
} guiwire_host_io_t;

typedef enum {
    GUIWIRE_HOST_IDLE = 0,
    GUIWIRE_HOST_WAIT_PING,
    GUIWIRE_HOST_WAIT_POLL,
    GUIWIRE_HOST_WAIT_LINK_INFO,
    GUIWIRE_HOST_WAIT_BAUD_ACK
} guiwire_host_wait_t;

typedef enum {
    GUIWIRE_RATE_OFF = 0,
    GUIWIRE_RATE_DISCOVER,
    GUIWIRE_RATE_READY,
    GUIWIRE_RATE_TRIAL,
    GUIWIRE_RATE_RECOVER,
    GUIWIRE_RATE_VERIFY_BASE
} guiwire_rate_stage_t;

typedef struct {
    guiwire_server_t server;
    guiwire_link_t link;
    guiwire_host_io_t io;
    guiwire_host_wait_t waiting;
    bool connected;
    bool initial_sync;
    uint8_t missed_responses;
    uint32_t now_ms;
    uint32_t last_valid_ms;
    uint32_t response_deadline_ms;
    uint32_t next_action_ms;
    uint32_t valid_packets;
    uint8_t last_command;
    uint32_t tx_frames;
    uint32_t session_resets;
    uint32_t current_baud;
    uint32_t previous_baud;
    uint32_t pending_baud;
    uint8_t baud_token;
    guiwire_rate_stage_t rate_stage;
    uint8_t common_rate_mask;
    uint8_t trial_good_responses;
    uint8_t rate_error_score;
    uint32_t next_rate_action_ms;
    uint32_t recovery_until_ms;
    uint32_t failed_rate_retry_ms;
    uint32_t failed_rate;
    uint32_t observed_invalid_frames;
    uint32_t rate_upshifts;
    uint32_t rate_downshifts;
    uint32_t rate_failures;
    bool rate_downshift_requested;
    bool trial_was_downshift;
    bool reliable_session;
    bool session_token_valid;
    bool has_acked_sequence;
    uint8_t session_token[GUIWIRE_SESSION_TOKEN_SIZE];
    uint16_t last_acked_sequence;
} guiwire_host_t;

guiwire_status_t guiwire_host_init(guiwire_host_t *host,
                                   const guiwire_server_config_t *server_cfg,
                                   const guiwire_host_io_t *io);

void guiwire_host_feed_byte(guiwire_host_t *host, uint8_t byte, uint32_t now_ms);
void guiwire_host_tick(guiwire_host_t *host, uint32_t now_ms);

bool guiwire_host_connected(const guiwire_host_t *host);
guiwire_server_t *guiwire_host_server(guiwire_host_t *host);

#ifdef __cplusplus
}
#endif

#endif
