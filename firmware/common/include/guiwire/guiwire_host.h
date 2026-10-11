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
    guiwire_host_set_baud_fn set_baud;
    void *set_baud_ctx;
} guiwire_host_io_t;

typedef enum {
    GUIWIRE_HOST_IDLE = 0,
    GUIWIRE_HOST_WAIT_PING,
    GUIWIRE_HOST_WAIT_POLL,
    GUIWIRE_HOST_WAIT_LINK_INFO,
    GUIWIRE_HOST_WAIT_BAUD_ACK
} guiwire_host_wait_t;

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
    uint32_t pending_baud;
    uint8_t baud_token;
    bool baud_negotiation_active;
    bool link_info_requested;
    bool set_baud_pending;
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
