/*
 * SPDX-License-Identifier: MIT
 * Authors: any:frequency, seige.digital GbR, Leander Seige (2026)
 */

#include <string.h>

#include "guiwire/guiwire_host.h"
#include "guiwire_protocol_internal.h"

static bool deadline_reached(uint32_t now_ms, uint32_t deadline_ms) {
    return (int32_t)(now_ms - deadline_ms) >= 0;
}

static uint32_t default_or(uint32_t value, uint32_t fallback) {
    return value ? value : fallback;
}

static guiwire_status_t host_send(guiwire_host_t *host,
                                  guiwire_command_t command,
                                  const uint8_t *payload,
                                  uint16_t payload_len) {
    guiwire_status_t st = guiwire_link_send_command(&host->link, command,
                                                     payload, payload_len);
    if (st != GUIWIRE_STATUS_OK) return st;
    host->tx_frames++;
    if (host->io.tx_complete) host->io.tx_complete(host->io.tx_complete_ctx);
    host->response_deadline_ms = host->now_ms + default_or(host->io.response_timeout_ms, 120u);
    return GUIWIRE_STATUS_OK;
}

static uint32_t baud_guard_ms(const guiwire_host_t *host) {
    return default_or(host->io.baud_guard_ms, 10u);
}

static void host_restore_base_baud(guiwire_host_t *host) {
    uint32_t base = host->io.base_baud;
    if (base && host->current_baud != base && host->io.set_baud) {
        (void)host->io.set_baud(host->io.set_baud_ctx, base);
    }
    if (base) host->current_baud = base;
}

static bool baud_is_supported(const uint32_t *bauds, uint8_t count, uint32_t baud) {
    uint8_t i;
    for (i = 0u; i < count; ++i) {
        if (bauds[i] == baud) return true;
    }
    return false;
}

static uint32_t highest_common_baud(const guiwire_host_t *host,
                                    const uint8_t *payload,
                                    uint8_t count) {
    uint32_t selected = 0u;
    uint8_t i;

    if (!host->io.supported_bauds || !host->io.supported_baud_count) return 0u;
    for (i = 0u; i < count; ++i) {
        uint32_t baud = guiwire_read_u32le(&payload[1u + (uint16_t)i * 4u]);
        if (baud_is_supported(host->io.supported_bauds,
                              host->io.supported_baud_count, baud) &&
            baud > selected) {
            selected = baud;
        }
    }
    return selected;
}

static void host_disconnect(guiwire_host_t *host) {
    host->session_resets++;
    host->connected = false;
    host->initial_sync = false;
    host->waiting = GUIWIRE_HOST_IDLE;
    host->missed_responses = 0u;
    host_restore_base_baud(host);
    host->baud_negotiation_active = false;
    host->link_info_requested = false;
    host->set_baud_pending = false;
    host->pending_baud = 0u;
    guiwire_link_reset(&host->link);
    (void)guiwire_server_reset_scene(&host->server);
    host->next_action_ms = host->now_ms;
}

static void host_packet(void *ctx, const uint8_t *packet, size_t len) {
    guiwire_host_t *host = (guiwire_host_t *)ctx;
    guiwire_packet_view_t view;
    guiwire_status_t st;

    if (!host || guiwire_protocol_parse(packet, len, &view) != GUIWIRE_STATUS_OK) return;

    host->valid_packets++;
    host->last_command = view.header.command;

    if (view.header.command == GUIWIRE_CMD_PING) {
        if (host->waiting != GUIWIRE_HOST_WAIT_PING) return;
        host->connected = true;
        host->initial_sync = true;
        host->missed_responses = 0u;
        host->last_valid_ms = host->now_ms;
        host->waiting = GUIWIRE_HOST_IDLE;
        host->baud_negotiation_active = host->io.set_baud &&
                                        host->io.supported_bauds &&
                                        host->io.supported_baud_count;
        host->link_info_requested = false;
        host->set_baud_pending = false;
        host->next_action_ms = host->now_ms + baud_guard_ms(host);
        (void)guiwire_server_reset_scene(&host->server);
        return;
    }

    if (view.header.command == GUIWIRE_CMD_LINK_INFO &&
        host->waiting == GUIWIRE_HOST_WAIT_LINK_INFO) {
        uint8_t count;
        uint32_t selected;

        if (view.header.payload_len < 1u) return;
        count = view.payload[0];
        if (!count || count > 8u || view.header.payload_len != (uint16_t)(1u + count * 4u)) {
            return;
        }
        host->last_valid_ms = host->now_ms;
        host->missed_responses = 0u;
        host->waiting = GUIWIRE_HOST_IDLE;
        selected = highest_common_baud(host, view.payload, count);
        if (!selected || selected == host->current_baud) {
            host->baud_negotiation_active = false;
            host->next_action_ms = host->now_ms + baud_guard_ms(host);
            return;
        }
        host->pending_baud = selected;
        host->baud_token = (uint8_t)(host->baud_token + 1u);
        if (!host->baud_token) host->baud_token = 1u;
        host->set_baud_pending = true;
        host->next_action_ms = host->now_ms + baud_guard_ms(host);
        return;
    }

    if (view.header.command == GUIWIRE_CMD_BAUD_ACK &&
        host->waiting == GUIWIRE_HOST_WAIT_BAUD_ACK) {
        uint32_t acknowledged_baud;
        guiwire_status_t baud_status;

        if (view.header.payload_len != 5u) return;
        acknowledged_baud = guiwire_read_u32le(view.payload);
        if (acknowledged_baud != host->pending_baud ||
            view.payload[4] != host->baud_token) return;
        baud_status = host->io.set_baud(host->io.set_baud_ctx, acknowledged_baud);
        if (baud_status != GUIWIRE_STATUS_OK) {
            host_disconnect(host);
            return;
        }
        host->current_baud = acknowledged_baud;
        host->last_valid_ms = host->now_ms;
        host->missed_responses = 0u;
        host->waiting = GUIWIRE_HOST_IDLE;
        host->baud_negotiation_active = false;
        host->set_baud_pending = false;
        host->next_action_ms = host->now_ms + baud_guard_ms(host);
        return;
    }

    if (!host->connected || host->waiting != GUIWIRE_HOST_WAIT_POLL) return;
    host->last_valid_ms = host->now_ms;
    host->missed_responses = 0u;
    host->waiting = GUIWIRE_HOST_IDLE;
    host->next_action_ms = host->now_ms + default_or(host->io.poll_interval_ms, 100u);

    if (view.header.command == GUIWIRE_CMD_NO_DATA) return;
    if (host->initial_sync && view.header.command != GUIWIRE_CMD_DESTROY_ALL_OBJECTS) return;

    st = guiwire_server_handle_packet(&host->server, packet, len);
    if (st == GUIWIRE_STATUS_OK && view.header.command == GUIWIRE_CMD_DESTROY_ALL_OBJECTS) {
        host->initial_sync = false;
    }
}

guiwire_status_t guiwire_host_init(guiwire_host_t *host,
                                   const guiwire_server_config_t *server_cfg,
                                   const guiwire_host_io_t *io) {
    guiwire_status_t st;

    if (!host || !server_cfg || !io || !io->write) return GUIWIRE_STATUS_ERR_INVALID_ARGUMENT;
    memset(host, 0, sizeof(*host));
    host->io = *io;
    host->current_baud = host->io.base_baud;
    st = guiwire_server_init(&host->server, server_cfg);
    if (st != GUIWIRE_STATUS_OK) return st;
    st = guiwire_server_show_waiting(&host->server);
    if (st != GUIWIRE_STATUS_OK) return st;
    guiwire_link_init(&host->link, host->io.write, host->io.write_ctx, host_packet, host);
    host->waiting = GUIWIRE_HOST_IDLE;
    return GUIWIRE_STATUS_OK;
}

void guiwire_host_feed_byte(guiwire_host_t *host, uint8_t byte, uint32_t now_ms) {
    if (!host) return;
    host->now_ms = now_ms;
    guiwire_link_feed_byte(&host->link, byte);
}

void guiwire_host_tick(guiwire_host_t *host, uint32_t now_ms) {
    guiwire_status_t st;
    uint32_t session_timeout;

    if (!host) return;
    host->now_ms = now_ms;
    session_timeout = default_or(host->io.session_timeout_ms, 500u);

    /* Handle an outstanding request before the general session timeout. This
     * is important during optional capability negotiation: a legacy Device
     * may deliberately remain silent for GET_LINK_INFO. */
    if (host->waiting != GUIWIRE_HOST_IDLE && deadline_reached(now_ms, host->response_deadline_ms)) {
        if (host->waiting == GUIWIRE_HOST_WAIT_LINK_INFO) {
            /* A legacy device does not know this command. Continue at the
             * base rate instead of making negotiation mandatory. */
            host->waiting = GUIWIRE_HOST_IDLE;
            host->baud_negotiation_active = false;
            host->next_action_ms = now_ms + baud_guard_ms(host);
        } else if (host->waiting == GUIWIRE_HOST_WAIT_BAUD_ACK) {
            host_disconnect(host);
        } else {
            host->waiting = GUIWIRE_HOST_IDLE;
            if (!host->connected) {
                host->next_action_ms = now_ms + default_or(host->io.discovery_interval_ms, 100u);
            } else if (++host->missed_responses >= (host->io.max_missed_responses ? host->io.max_missed_responses : 3u)) {
                host_disconnect(host);
            } else {
                host->next_action_ms = now_ms + 20u;
            }
        }
    } else if (host->connected && now_ms - host->last_valid_ms >= session_timeout) {
        host_disconnect(host);
    }

    if (host->waiting != GUIWIRE_HOST_IDLE || !deadline_reached(now_ms, host->next_action_ms)) return;

    if (host->connected && host->baud_negotiation_active) {
        if (host->set_baud_pending) {
            uint8_t payload[5];
            guiwire_write_u32le(payload, host->pending_baud);
            payload[4] = host->baud_token;
            host->waiting = GUIWIRE_HOST_WAIT_BAUD_ACK;
            st = host_send(host, GUIWIRE_CMD_SET_BAUD, payload, sizeof(payload));
            host->response_deadline_ms = now_ms + 250u;
        } else if (!host->link_info_requested) {
            host->link_info_requested = true;
            host->waiting = GUIWIRE_HOST_WAIT_LINK_INFO;
            st = host_send(host, GUIWIRE_CMD_GET_LINK_INFO, NULL, 0u);
            host->response_deadline_ms = now_ms + 250u;
        } else {
            host->baud_negotiation_active = false;
            host->next_action_ms = now_ms + baud_guard_ms(host);
            return;
        }
        if (st != GUIWIRE_STATUS_OK) host_disconnect(host);
        return;
    }

    host->waiting = host->connected ? GUIWIRE_HOST_WAIT_POLL : GUIWIRE_HOST_WAIT_PING;
    st = host_send(host, host->connected ? GUIWIRE_CMD_POLL : GUIWIRE_CMD_PING, NULL, 0u);
    if (st != GUIWIRE_STATUS_OK) host_disconnect(host);
}

bool guiwire_host_connected(const guiwire_host_t *host) {
    return host && host->connected;
}

guiwire_server_t *guiwire_host_server(guiwire_host_t *host) {
    return host ? &host->server : NULL;
}
