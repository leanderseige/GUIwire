/*
 * SPDX-License-Identifier: MIT
 * Authors: any:frequency, seige.digital GbR, Leander Seige (2026)
 */

#include <string.h>

#include "guiwire/guiwire_host.h"
#include "guiwire_protocol_internal.h"

#define GUIWIRE_RATE_DISCOVERY_DELAY_MS 1500u
#define GUIWIRE_RATE_STEP_INTERVAL_MS 5000u
#define GUIWIRE_RATE_FAILED_RETRY_MS 30000u
#define GUIWIRE_RATE_FAST_RESPONSE_MS 200u
#define GUIWIRE_RATE_ACK_TIMEOUT_MS 250u
#define GUIWIRE_RATE_TRIAL_GOOD_RESPONSES 3u

static bool deadline_reached(uint32_t now_ms, uint32_t deadline_ms) {
    return (int32_t)(now_ms - deadline_ms) >= 0;
}

static uint32_t default_or(uint32_t value, uint32_t fallback) {
    return value ? value : fallback;
}

static void host_refresh_time(guiwire_host_t *host) {
    if (host->io.read_time_ms) host->now_ms = host->io.read_time_ms(host->io.time_ctx);
}

static guiwire_status_t host_send(guiwire_host_t *host,
                                  guiwire_command_t command,
                                  const uint8_t *payload,
                                  uint16_t payload_len) {
    guiwire_status_t st = guiwire_link_send_command(&host->link, command,
                                                     payload, payload_len);
    uint32_t configured_timeout;
    uint32_t baud;
    uint32_t worst_wire_ms;
    if (st != GUIWIRE_STATUS_OK) return st;
    host->tx_frames++;
    configured_timeout = host->current_baud > host->io.base_baud
        ? GUIWIRE_RATE_FAST_RESPONSE_MS
        : default_or(host->io.response_timeout_ms, 120u);
    baud = host->current_baud ? host->current_baud : 9600u;
    /* SYNC + worst-case stuffing of length, full packet and CRC. Include
     * our request's turnaround and a processing margin in the 75 ms guard. */
    worst_wire_ms = ((2u * GUIWIRE_MAX_PACKET_SIZE + 9u) * 10000u +
                     baud - 1u) / baud + 75u;
    if (configured_timeout < worst_wire_ms) configured_timeout = worst_wire_ms;
    host->response_deadline_ms = host->now_ms + configured_timeout;
    if (host->io.tx_complete) host->io.tx_complete(host->io.tx_complete_ctx);
    return GUIWIRE_STATUS_OK;
}

static uint32_t baud_guard_ms(const guiwire_host_t *host) {
    return default_or(host->io.baud_guard_ms, 10u);
}

static uint32_t baud_fallback_ms(const guiwire_host_t *host) {
    return default_or(host->io.baud_fallback_ms, 400u);
}

static void host_restore_base_baud(guiwire_host_t *host) {
    uint32_t base = host->io.base_baud;
    if (base && host->current_baud != base && host->io.set_baud) {
        (void)host->io.set_baud(host->io.set_baud_ctx, base);
    }
    if (base) host->current_baud = base;
}

static uint8_t common_baud_mask(const guiwire_host_t *host,
                                const uint8_t *payload, uint8_t count) {
    uint8_t mask = 0u;
    uint8_t local_count = host->io.supported_baud_count;
    if (local_count > 8u) local_count = 8u;
    for (uint8_t i = 0u; i < local_count; ++i) {
        for (uint8_t j = 0u; j < count; ++j) {
            if (host->io.supported_bauds[i] ==
                guiwire_read_u32le(&payload[1u + (uint16_t)j * 4u])) {
                mask |= (uint8_t)(1u << i);
                break;
            }
        }
    }
    return mask;
}

static uint32_t next_higher_baud(const guiwire_host_t *host) {
    uint32_t selected = 0u;
    uint8_t count = host->io.supported_baud_count;
    if (count > 8u) count = 8u;
    for (uint8_t i = 0u; i < count; ++i) {
        uint32_t baud = host->io.supported_bauds[i];
        if (!(host->common_rate_mask & (1u << i)) || baud <= host->current_baud) continue;
        if (!selected || baud < selected) selected = baud;
    }
    /* Do not skip a failed intermediate step to try an even faster one. */
    if (selected == host->failed_rate &&
        !deadline_reached(host->now_ms, host->failed_rate_retry_ms)) return 0u;
    return selected;
}

static uint32_t next_lower_baud(const guiwire_host_t *host) {
    uint32_t selected = host->io.base_baud;
    uint8_t count = host->io.supported_baud_count;
    if (count > 8u) count = 8u;
    for (uint8_t i = 0u; i < count; ++i) {
        uint32_t baud = host->io.supported_bauds[i];
        if ((host->common_rate_mask & (1u << i)) &&
            baud < host->current_baud && baud > selected) selected = baud;
    }
    return selected;
}

static void host_enter_rate_recovery(guiwire_host_t *host) {
    /* A lost ACK while stepping down is evidence against the current faster
     * rate, not against the slower destination that we never verified. */
    uint32_t failed = host->current_baud;
    if (host->pending_baud > failed) failed = host->pending_baud;
    if (failed > host->io.base_baud) {
        host->failed_rate = failed;
        host->failed_rate_retry_ms = host->now_ms + GUIWIRE_RATE_FAILED_RETRY_MS;
    }
    host->rate_failures++;
    host->rate_stage = GUIWIRE_RATE_RECOVER;
    host->waiting = GUIWIRE_HOST_IDLE;
    host->missed_responses = 0u;
    host->pending_baud = 0u;
    host->rate_downshift_requested = false;
    /* Remain silent long enough for the Device's high-rate watchdog to
     * return to the mandatory startup rate without losing its GUI queue. */
    host->recovery_until_ms = host->now_ms + baud_fallback_ms(host) + 50u;
    guiwire_link_reset(&host->link);
}

static void host_disconnect(guiwire_host_t *host) {
    host->session_resets++;
    host->connected = false;
    host->initial_sync = false;
    host->reliable_session = false;
    host->has_acked_sequence = false;
    host->waiting = GUIWIRE_HOST_IDLE;
    host->missed_responses = 0u;
    host_restore_base_baud(host);
    host->rate_stage = GUIWIRE_RATE_OFF;
    host->common_rate_mask = 0u;
    host->rate_downshift_requested = false;
    host->trial_good_responses = 0u;
    host->rate_error_score = 0u;
    host->pending_baud = 0u;
    guiwire_link_reset(&host->link);
    (void)guiwire_server_reset_scene(&host->server);
    host_refresh_time(host);
    /* The peer has just transmitted MISMATCH; let its TX/echo guard expire
     * before sending the new PING. */
    host->next_action_ms = host->now_ms + baud_guard_ms(host) + 20u;
}

static void host_packet(void *ctx, const uint8_t *packet, size_t len) {
    guiwire_host_t *host = (guiwire_host_t *)ctx;
    guiwire_packet_view_t view;
    guiwire_status_t st;
    uint8_t object_packet[GUIWIRE_MAX_PACKET_SIZE];
    size_t object_packet_len = 0u;
    uint8_t effective_command;
    bool new_sequence = false;
    uint16_t sequence = 0u;

    if (!host || guiwire_protocol_parse(packet, len, &view) != GUIWIRE_STATUS_OK) return;

    host->valid_packets++;
    host->last_command = view.header.command;

    if (view.header.command == GUIWIRE_CMD_PING) {
        if (host->waiting != GUIWIRE_HOST_WAIT_PING) return;
        if (view.header.payload_len == GUIWIRE_SESSION_TOKEN_SIZE &&
            host->session_token_valid &&
            memcmp(view.payload, host->session_token, GUIWIRE_SESSION_TOKEN_SIZE) == 0) {
            host->reliable_session = true;
        } else if (view.header.payload_len == 0u && !host->io.require_reliable_session) {
            host->reliable_session = false;
        } else {
            return;
        }
        host->has_acked_sequence = false;
        host->last_acked_sequence = GUIWIRE_SESSION_NO_ACK;
        host->connected = true;
        host->initial_sync = true;
        host->missed_responses = 0u;
        host->waiting = GUIWIRE_HOST_IDLE;
        host->rate_stage = (host->io.set_baud && host->io.supported_bauds &&
                            host->io.supported_baud_count)
            ? GUIWIRE_RATE_DISCOVER : GUIWIRE_RATE_OFF;
        host->common_rate_mask = 0u;
        host->rate_error_score = 0u;
        host->rate_downshift_requested = false;
        host->failed_rate = 0u;
        (void)guiwire_server_reset_scene(&host->server);
        host_refresh_time(host);
        host->last_valid_ms = host->now_ms;
        host->next_rate_action_ms = host->now_ms + GUIWIRE_RATE_DISCOVERY_DELAY_MS;
        host->next_action_ms = host->now_ms + baud_guard_ms(host);
        return;
    }

    if (view.header.command == GUIWIRE_CMD_SESSION_MISMATCH &&
        host->connected && host->reliable_session &&
        view.header.payload_len == GUIWIRE_SESSION_TOKEN_SIZE &&
        memcmp(view.payload, host->session_token, GUIWIRE_SESSION_TOKEN_SIZE) == 0) {
        host_disconnect(host);
        return;
    }

    if (view.header.command == GUIWIRE_CMD_LINK_INFO &&
        host->waiting == GUIWIRE_HOST_WAIT_LINK_INFO) {
        uint8_t count;
        uint8_t offset = host->reliable_session ? GUIWIRE_SESSION_TOKEN_SIZE : 0u;
        if (view.header.payload_len < (uint16_t)(offset + 1u) ||
            (offset && memcmp(view.payload, host->session_token, offset) != 0)) return;
        count = view.payload[offset];
        if (!count || count > 8u ||
            view.header.payload_len != (uint16_t)(offset + 1u + count * 4u)) {
            return;
        }
        host->last_valid_ms = host->now_ms;
        host->missed_responses = 0u;
        host->waiting = GUIWIRE_HOST_IDLE;
        host->common_rate_mask = common_baud_mask(host, view.payload + offset, count);
        host->rate_stage = host->common_rate_mask ? GUIWIRE_RATE_READY : GUIWIRE_RATE_OFF;
        host->next_rate_action_ms = host->now_ms + 100u;
        host->next_action_ms = host->now_ms + baud_guard_ms(host);
        return;
    }

    if (view.header.command == GUIWIRE_CMD_BAUD_ACK &&
        host->waiting == GUIWIRE_HOST_WAIT_BAUD_ACK) {
        uint32_t acknowledged_baud;
        guiwire_status_t baud_status;
        uint8_t offset = host->reliable_session ? GUIWIRE_SESSION_TOKEN_SIZE : 0u;

        if (view.header.payload_len != (uint16_t)(offset + 5u) ||
            (offset && memcmp(view.payload, host->session_token, offset) != 0)) return;
        acknowledged_baud = guiwire_read_u32le(view.payload + offset);
        if (acknowledged_baud != host->pending_baud ||
            view.payload[offset + 4u] != host->baud_token) return;
        baud_status = host->io.set_baud(host->io.set_baud_ctx, acknowledged_baud);
        if (baud_status != GUIWIRE_STATUS_OK) {
            host_enter_rate_recovery(host);
            return;
        }
        host->previous_baud = host->current_baud;
        host->trial_was_downshift = acknowledged_baud < host->current_baud;
        host->current_baud = acknowledged_baud;
        host->last_valid_ms = host->now_ms;
        host->missed_responses = 0u;
        host->waiting = GUIWIRE_HOST_IDLE;
        host->rate_stage = GUIWIRE_RATE_TRIAL;
        host->trial_good_responses = 0u;
        host->rate_error_score = 0u;
        host->pending_baud = 0u;
        host->next_action_ms = host->now_ms + baud_guard_ms(host);
        return;
    }

    if (!host->connected || host->waiting != GUIWIRE_HOST_WAIT_POLL) return;
    effective_command = view.header.command;
    if (host->reliable_session) {
        if (view.header.command == GUIWIRE_CMD_SESSION_EMPTY &&
            view.header.payload_len == GUIWIRE_SESSION_TOKEN_SIZE &&
            memcmp(view.payload, host->session_token, GUIWIRE_SESSION_TOKEN_SIZE) == 0) {
            effective_command = GUIWIRE_CMD_NO_DATA;
        } else if (view.header.command == GUIWIRE_CMD_SESSION_DATA &&
                   view.header.payload_len >= GUIWIRE_SESSION_DATA_OVERHEAD &&
                   memcmp(view.payload, host->session_token, GUIWIRE_SESSION_TOKEN_SIZE) == 0) {
            uint16_t expected = host->has_acked_sequence
                ? (uint16_t)(host->last_acked_sequence + 1u) : 0u;
            if (expected == GUIWIRE_SESSION_NO_ACK) expected = 0u;
            sequence = guiwire_read_u16le(&view.payload[8]);
            effective_command = view.payload[10];
            if (host->has_acked_sequence && sequence == host->last_acked_sequence) {
                /* ACK was lost; do not execute the object command twice. */
                effective_command = GUIWIRE_CMD_NO_DATA;
            } else if (sequence != expected) {
                host_disconnect(host);
                return;
            } else {
                if (effective_command != GUIWIRE_CMD_HELLO_STRING &&
                    (effective_command < GUIWIRE_CMD_DESTROY_ALL_OBJECTS ||
                     effective_command > GUIWIRE_CMD_UPDATE_RING)) {
                    host_disconnect(host);
                    return;
                }
                st = guiwire_protocol_encode((guiwire_command_t)effective_command,
                      &view.payload[GUIWIRE_SESSION_DATA_OVERHEAD],
                      (uint16_t)(view.header.payload_len - GUIWIRE_SESSION_DATA_OVERHEAD),
                      object_packet, sizeof(object_packet), &object_packet_len);
                if (st != GUIWIRE_STATUS_OK) {
                    host_disconnect(host);
                    return;
                }
                packet = object_packet;
                len = object_packet_len;
                new_sequence = true;
            }
        } else {
            return;
        }
    } else if (view.header.command == GUIWIRE_CMD_SESSION_DATA ||
               view.header.command == GUIWIRE_CMD_SESSION_EMPTY) {
        return;
    }
    host->last_command = effective_command;
    if (host->reliable_session && host->initial_sync &&
        view.header.command == GUIWIRE_CMD_SESSION_EMPTY) {
        /* A conforming new session always starts with DESTROY_ALL_OBJECTS.
         * An empty snapshot cannot be repaired by further polling. */
        host_disconnect(host);
        return;
    }
    if (effective_command == GUIWIRE_CMD_NO_DATA) {
        st = GUIWIRE_STATUS_OK;
    } else if (effective_command >= GUIWIRE_CMD_DESTROY_ALL_OBJECTS &&
               effective_command <= GUIWIRE_CMD_UPDATE_RING &&
               (!host->initial_sync || effective_command == GUIWIRE_CMD_DESTROY_ALL_OBJECTS)) {
        st = guiwire_server_handle_packet(&host->server, packet, len);
    } else if (effective_command == GUIWIRE_CMD_HELLO_STRING && !host->initial_sync) {
        st = guiwire_server_handle_packet(&host->server, packet, len);
    } else {
        if (host->reliable_session) host_disconnect(host);
        return;
    }
    if (st != GUIWIRE_STATUS_OK) {
        if (host->reliable_session) host_disconnect(host);
        return;
    }
    if (new_sequence) {
        host->last_acked_sequence = sequence;
        host->has_acked_sequence = true;
    }

    host_refresh_time(host);
    host->last_valid_ms = host->now_ms;
    host->missed_responses = 0u;
    host->waiting = GUIWIRE_HOST_IDLE;
    host->next_action_ms = host->now_ms + default_or(host->io.poll_interval_ms, 100u);
    if (effective_command == GUIWIRE_CMD_DESTROY_ALL_OBJECTS) {
        host->initial_sync = false;
    }
    if (host->rate_stage == GUIWIRE_RATE_TRIAL) {
        if (++host->trial_good_responses >= GUIWIRE_RATE_TRIAL_GOOD_RESPONSES) {
            host->rate_stage = GUIWIRE_RATE_READY;
            if (host->trial_was_downshift) {
                host->rate_downshifts++;
                host->failed_rate = host->previous_baud;
                host->failed_rate_retry_ms = host->now_ms + GUIWIRE_RATE_FAILED_RETRY_MS;
                host->next_rate_action_ms = host->failed_rate_retry_ms;
            } else {
                host->rate_upshifts++;
                host->next_rate_action_ms = host->now_ms + GUIWIRE_RATE_STEP_INTERVAL_MS;
            }
        }
    } else if (host->rate_stage == GUIWIRE_RATE_VERIFY_BASE) {
        host->rate_stage = host->common_rate_mask ? GUIWIRE_RATE_READY : GUIWIRE_RATE_OFF;
        host->next_rate_action_ms = host->now_ms + GUIWIRE_RATE_STEP_INTERVAL_MS;
    }
    if (host->rate_error_score) host->rate_error_score--;
}

guiwire_status_t guiwire_host_init(guiwire_host_t *host,
                                   const guiwire_server_config_t *server_cfg,
                                   const guiwire_host_io_t *io) {
    guiwire_status_t st;

    if (!host || !server_cfg || !io || !io->write ||
        (io->require_reliable_session && !io->new_session_token)) {
        return GUIWIRE_STATUS_ERR_INVALID_ARGUMENT;
    }
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

static guiwire_status_t host_request_baud(guiwire_host_t *host, uint32_t baud) {
    uint8_t payload[13];
    uint8_t offset = host->reliable_session ? GUIWIRE_SESSION_TOKEN_SIZE : 0u;
    guiwire_status_t st;
    host->pending_baud = baud;
    host->baud_token = (uint8_t)(host->baud_token + 1u);
    if (!host->baud_token) host->baud_token = 1u;
    if (offset) memcpy(payload, host->session_token, offset);
    guiwire_write_u32le(payload + offset, baud);
    payload[offset + 4u] = host->baud_token;
    host->waiting = GUIWIRE_HOST_WAIT_BAUD_ACK;
    st = host_send(host, GUIWIRE_CMD_SET_BAUD, payload, (uint16_t)(offset + 5u));
    host->response_deadline_ms = host->now_ms + GUIWIRE_RATE_ACK_TIMEOUT_MS;
    return st;
}

void guiwire_host_tick(guiwire_host_t *host, uint32_t now_ms) {
    guiwire_status_t st;
    uint32_t session_timeout;

    if (!host) return;
    host->now_ms = now_ms;
    session_timeout = default_or(host->io.session_timeout_ms, 500u);

    if (host->link.invalid_frames != host->observed_invalid_frames) {
        host->observed_invalid_frames = host->link.invalid_frames;
        if (host->rate_stage == GUIWIRE_RATE_TRIAL) host->trial_good_responses = 0u;
        if (host->rate_error_score < 3u) host->rate_error_score++;
        if (host->rate_stage == GUIWIRE_RATE_READY &&
            host->current_baud > host->io.base_baud &&
            host->rate_error_score >= 2u) {
            host->rate_downshift_requested = true;
        }
    }

    if (host->rate_stage == GUIWIRE_RATE_RECOVER) {
        if (!deadline_reached(now_ms, host->recovery_until_ms)) return;
        host_restore_base_baud(host);
        guiwire_link_reset(&host->link);
        host->rate_stage = GUIWIRE_RATE_VERIFY_BASE;
        host->last_valid_ms = now_ms;
        host->next_action_ms = now_ms + baud_guard_ms(host);
    }

    if (host->waiting != GUIWIRE_HOST_IDLE && deadline_reached(now_ms, host->response_deadline_ms)) {
        if (host->waiting == GUIWIRE_HOST_WAIT_LINK_INFO) {
            host->waiting = GUIWIRE_HOST_IDLE;
            host->rate_stage = GUIWIRE_RATE_OFF;
            host->next_action_ms = now_ms + baud_guard_ms(host);
        } else if (host->waiting == GUIWIRE_HOST_WAIT_BAUD_ACK) {
            host_enter_rate_recovery(host);
        } else if (host->waiting == GUIWIRE_HOST_WAIT_PING) {
            host->waiting = GUIWIRE_HOST_IDLE;
            host->next_action_ms = now_ms + default_or(host->io.discovery_interval_ms, 100u);
        } else {
            host->waiting = GUIWIRE_HOST_IDLE;
            if (host->rate_stage == GUIWIRE_RATE_TRIAL) host->trial_good_responses = 0u;
            if (host->rate_error_score < 2u) host->rate_error_score = 2u;
            if (host->rate_stage == GUIWIRE_RATE_READY &&
                host->current_baud > host->io.base_baud) {
                host->rate_downshift_requested = true;
            }
            if (host->current_baud == host->io.base_baud &&
                host->rate_stage == GUIWIRE_RATE_READY) {
                host->next_rate_action_ms = now_ms + GUIWIRE_RATE_FAILED_RETRY_MS;
            }
            host->missed_responses++;
            if (host->rate_stage == GUIWIRE_RATE_VERIFY_BASE && host->missed_responses >= 2u) {
                host_disconnect(host);
            } else if (host->current_baud > host->io.base_baud &&
                       host->missed_responses >= 2u) {
                host_enter_rate_recovery(host);
            } else if (host->missed_responses >=
                       (host->io.max_missed_responses ? host->io.max_missed_responses : 3u)) {
                host_disconnect(host);
            } else {
                host->next_action_ms = now_ms + 20u;
            }
        }
    } else if (host->connected && host->current_baud == host->io.base_baud &&
               host->rate_stage != GUIWIRE_RATE_VERIFY_BASE &&
               now_ms - host->last_valid_ms >= session_timeout) {
        host_disconnect(host);
    }

    if (host->rate_stage == GUIWIRE_RATE_RECOVER) return;
    if (host->waiting != GUIWIRE_HOST_IDLE || !deadline_reached(now_ms, host->next_action_ms)) return;

    if (host->connected && !host->initial_sync &&
        host->rate_stage == GUIWIRE_RATE_DISCOVER &&
        deadline_reached(now_ms, host->next_rate_action_ms)) {
        host->waiting = GUIWIRE_HOST_WAIT_LINK_INFO;
        st = host_send(host, GUIWIRE_CMD_GET_LINK_INFO,
                       host->reliable_session ? host->session_token : NULL,
                       host->reliable_session ? GUIWIRE_SESSION_TOKEN_SIZE : 0u);
        host->response_deadline_ms = now_ms + GUIWIRE_RATE_ACK_TIMEOUT_MS;
        if (st != GUIWIRE_STATUS_OK) host_disconnect(host);
        return;
    }

    if (host->connected && !host->initial_sync &&
        host->rate_stage == GUIWIRE_RATE_READY) {
        if (host->rate_downshift_requested && host->missed_responses == 0u &&
            host->current_baud > host->io.base_baud) {
            host->rate_downshift_requested = false;
            st = host_request_baud(host, next_lower_baud(host));
            if (st != GUIWIRE_STATUS_OK) host_disconnect(host);
            return;
        }
        if (deadline_reached(now_ms, host->next_rate_action_ms)) {
            uint32_t next = next_higher_baud(host);
            host->next_rate_action_ms = now_ms + GUIWIRE_RATE_STEP_INTERVAL_MS;
            if (next) {
                st = host_request_baud(host, next);
                if (st != GUIWIRE_STATUS_OK) host_disconnect(host);
                return;
            }
        }
    }

    host->waiting = host->connected ? GUIWIRE_HOST_WAIT_POLL : GUIWIRE_HOST_WAIT_PING;
    if (host->connected && host->reliable_session) {
        uint8_t payload[10];
        memcpy(payload, host->session_token, GUIWIRE_SESSION_TOKEN_SIZE);
        guiwire_write_u16le(&payload[8], host->has_acked_sequence
                            ? host->last_acked_sequence : GUIWIRE_SESSION_NO_ACK);
        st = host_send(host, GUIWIRE_CMD_POLL, payload, sizeof(payload));
    } else if (!host->connected && host->io.new_session_token) {
        uint8_t previous[GUIWIRE_SESSION_TOKEN_SIZE];
        memcpy(previous, host->session_token, sizeof(previous));
        host->io.new_session_token(host->io.session_token_ctx, host->session_token);
        if (host->session_token_valid &&
            memcmp(previous, host->session_token, sizeof(previous)) == 0) {
            host->session_token[0]++;
        }
        host->session_token_valid = true;
        st = host_send(host, GUIWIRE_CMD_PING, host->session_token,
                       GUIWIRE_SESSION_TOKEN_SIZE);
    } else {
        st = host_send(host, host->connected ? GUIWIRE_CMD_POLL : GUIWIRE_CMD_PING,
                       NULL, 0u);
    }
    if (st != GUIWIRE_STATUS_OK) host_disconnect(host);
}

bool guiwire_host_connected(const guiwire_host_t *host) {
    return host && host->connected;
}

guiwire_server_t *guiwire_host_server(guiwire_host_t *host) {
    return host ? &host->server : NULL;
}
