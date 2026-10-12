/*
 * SPDX-License-Identifier: MIT
 * Authors: any:frequency, seige.digital GbR, Leander Seige (2026)
 */

#include <string.h>

#include "guiwire/guiwire_device_runtime.h"
#include "guiwire_protocol_internal.h"

static bool deadline_reached(uint32_t now_ms, uint32_t deadline_ms) {
    return (int32_t)(now_ms - deadline_ms) >= 0;
}

static guiwire_status_t runtime_send_packet(guiwire_device_runtime_t *runtime,
                                            const uint8_t *packet,
                                            size_t len) {
    guiwire_status_t st;
    if (runtime->io.tx_begin) runtime->io.tx_begin(runtime->io.tx_begin_ctx);
    st = guiwire_link_send_packet(&runtime->link, packet, len);
    if (st != GUIWIRE_STATUS_OK) return st;
    if (runtime->io.tx_complete) runtime->io.tx_complete(runtime->io.tx_complete_ctx);
    runtime->tx_guard_until_ms = runtime->now_ms +
        (runtime->io.echo_guard_ms ? runtime->io.echo_guard_ms : 20u);
    return GUIWIRE_STATUS_OK;
}

static guiwire_status_t runtime_send_command(guiwire_device_runtime_t *runtime,
                                             guiwire_command_t command) {
    guiwire_status_t st;
    if (runtime->io.tx_begin) runtime->io.tx_begin(runtime->io.tx_begin_ctx);
    st = guiwire_link_send_command(&runtime->link, command, NULL, 0u);
    if (st != GUIWIRE_STATUS_OK) return st;
    if (runtime->io.tx_complete) runtime->io.tx_complete(runtime->io.tx_complete_ctx);
    runtime->tx_guard_until_ms = runtime->now_ms +
        (runtime->io.echo_guard_ms ? runtime->io.echo_guard_ms : 20u);
    return GUIWIRE_STATUS_OK;
}

static guiwire_status_t runtime_send_command_payload(guiwire_device_runtime_t *runtime,
                                                     guiwire_command_t command,
                                                     const uint8_t *payload,
                                                     uint16_t payload_len) {
    guiwire_status_t st;
    if (runtime->io.tx_begin) runtime->io.tx_begin(runtime->io.tx_begin_ctx);
    st = guiwire_link_send_command(&runtime->link, command, payload, payload_len);
    if (st != GUIWIRE_STATUS_OK) return st;
    if (runtime->io.tx_complete) runtime->io.tx_complete(runtime->io.tx_complete_ctx);
    runtime->tx_guard_until_ms = runtime->now_ms +
        (runtime->io.echo_guard_ms ? runtime->io.echo_guard_ms : 20u);
    return GUIWIRE_STATUS_OK;
}

static bool runtime_baud_supported(const guiwire_device_runtime_t *runtime,
                                   uint32_t baud) {
    uint8_t i;
    for (i = 0u; i < runtime->io.supported_baud_count; ++i) {
        if (runtime->io.supported_bauds[i] == baud) return true;
    }
    return false;
}

static void runtime_restore_base_baud(guiwire_device_runtime_t *runtime) {
    uint32_t base = runtime->io.base_baud;
    if (base && runtime->current_baud != base && runtime->io.set_baud) {
        (void)runtime->io.set_baud(runtime->io.set_baud_ctx, base);
    }
    if (base) runtime->current_baud = base;
    runtime->baud_fallback_active = false;
}

static void runtime_note_authenticated_host(guiwire_device_runtime_t *runtime) {
    runtime->last_host_ms = runtime->now_ms;
    if (runtime->baud_fallback_active) {
        runtime->baud_fallback_deadline_ms = runtime->now_ms +
            (runtime->io.baud_fallback_ms ? runtime->io.baud_fallback_ms : 1000u);
    }
}

/* On small MCUs this packet buffer must not share the PING/session-start
 * stack frame.  The snapshot callback creates several objects and its stack
 * use is already substantial on an ATmega328P. */
#if defined(__GNUC__)
__attribute__((noinline))
#endif
static void runtime_reply_to_poll(guiwire_device_runtime_t *runtime) {
    uint8_t response[GUIWIRE_MAX_PACKET_SIZE];
    size_t response_len;

    runtime->last_host_ms = runtime->now_ms;
    if (guiwire_device_pop(&runtime->device, response, sizeof(response), &response_len)) {
        (void)runtime_send_packet(runtime, response, response_len);
    } else {
        (void)runtime_send_command(runtime, GUIWIRE_CMD_NO_DATA);
    }
}

/* The first queued object remains pending until the next POLL carries its
 * sequence as an ACK. A lost/corrupt response is therefore sent again. */
#if defined(__GNUC__)
__attribute__((noinline))
#endif
static void runtime_reply_to_session_poll(guiwire_device_runtime_t *runtime,
                                          uint16_t acknowledged_sequence) {
    const uint8_t *queued;
    size_t queued_len;
    guiwire_packet_view_t view;
    uint8_t payload[GUIWIRE_MAX_PACKET_SIZE - sizeof(guiwire_packet_header_t)];

    if (runtime->inflight && acknowledged_sequence == runtime->next_sequence) {
        (void)guiwire_device_commit(&runtime->device);
        runtime->next_sequence++;
        /* 0xFFFF is reserved for "no ACK yet" on session start. */
        if (runtime->next_sequence == GUIWIRE_SESSION_NO_ACK) runtime->next_sequence = 0u;
        runtime->inflight = false;
    }
    runtime->last_host_ms = runtime->now_ms;
    memcpy(payload, runtime->session_token, GUIWIRE_SESSION_TOKEN_SIZE);
    if (!guiwire_device_peek(&runtime->device, &queued, &queued_len)) {
        (void)runtime_send_command_payload(runtime, GUIWIRE_CMD_SESSION_EMPTY,
                                           payload, GUIWIRE_SESSION_TOKEN_SIZE);
        return;
    }
    if (guiwire_protocol_parse(queued, queued_len, &view) != GUIWIRE_STATUS_OK ||
        view.header.payload_len + GUIWIRE_SESSION_DATA_OVERHEAD +
            sizeof(guiwire_packet_header_t) > GUIWIRE_MAX_PACKET_SIZE) return;

    guiwire_write_u16le(&payload[8], runtime->next_sequence);
    payload[10] = view.header.command;
    memcpy(&payload[11], view.payload, view.header.payload_len);
    if (runtime_send_command_payload(runtime, GUIWIRE_CMD_SESSION_DATA, payload,
                                     (uint16_t)(11u + view.header.payload_len)) == GUIWIRE_STATUS_OK) {
        runtime->inflight = true;
    }
}

static bool runtime_session_matches(const guiwire_device_runtime_t *runtime,
                                    const uint8_t *token) {
    return runtime->reliable_session &&
           guiwire_device_session_active(&runtime->device) &&
           memcmp(token, runtime->session_token, GUIWIRE_SESSION_TOKEN_SIZE) == 0;
}

static void runtime_session_mismatch(guiwire_device_runtime_t *runtime,
                                     const uint8_t *request_token) {
    (void)runtime_send_command_payload(runtime, GUIWIRE_CMD_SESSION_MISMATCH,
                                       request_token, GUIWIRE_SESSION_TOKEN_SIZE);
}

static void runtime_packet(void *ctx, const uint8_t *packet, size_t len) {
    guiwire_device_runtime_t *runtime = (guiwire_device_runtime_t *)ctx;
    guiwire_packet_view_t view;

    if (!runtime || guiwire_protocol_parse(packet, len, &view) != GUIWIRE_STATUS_OK) return;

    if (view.header.command == GUIWIRE_CMD_PING) {
        if (!deadline_reached(runtime->now_ms, runtime->tx_guard_until_ms)) return;
        if (view.header.payload_len != 0u &&
            view.header.payload_len != GUIWIRE_SESSION_TOKEN_SIZE) return;
        /* Every PING starts a fresh display session, including a fast
         * reconnect before the previous session timeout has elapsed. */
        guiwire_device_clear_queue(&runtime->device);
        guiwire_device_set_session(&runtime->device, true);
        runtime->reliable_session = view.header.payload_len == GUIWIRE_SESSION_TOKEN_SIZE;
        runtime->inflight = false;
        runtime->next_sequence = 0u;
        if (runtime->reliable_session) {
            memcpy(runtime->session_token, view.payload, GUIWIRE_SESSION_TOKEN_SIZE);
        }
        if (runtime->on_session_start) {
            runtime->on_session_start(runtime->session_ctx, &runtime->device);
        }
        runtime_note_authenticated_host(runtime);
        (void)runtime_send_command_payload(runtime, GUIWIRE_CMD_PING,
                                           runtime->reliable_session ? runtime->session_token : NULL,
                                           runtime->reliable_session ? GUIWIRE_SESSION_TOKEN_SIZE : 0u);
        return;
    }

    if (view.header.command == GUIWIRE_CMD_GET_LINK_INFO) {
        uint8_t response[41];
        uint8_t count = runtime->io.supported_baud_count;
        uint8_t offset = runtime->reliable_session ? GUIWIRE_SESSION_TOKEN_SIZE : 0u;
        uint8_t i;

        if (view.header.payload_len == GUIWIRE_SESSION_TOKEN_SIZE &&
            !runtime_session_matches(runtime, view.payload)) {
            runtime_session_mismatch(runtime, view.payload);
            return;
        }
        if (!guiwire_device_session_active(&runtime->device) ||
            view.header.payload_len != offset) return;
        runtime_note_authenticated_host(runtime);
        if (!runtime->io.supported_bauds || !count) return;
        if (count > 8u) count = 8u;
        if (offset) memcpy(response, runtime->session_token, offset);
        response[offset] = count;
        for (i = 0u; i < count; ++i) {
            guiwire_write_u32le(&response[offset + 1u + (uint16_t)i * 4u],
                                runtime->io.supported_bauds[i]);
        }
        (void)runtime_send_command_payload(runtime, GUIWIRE_CMD_LINK_INFO,
                                           response, (uint16_t)(offset + 1u + count * 4u));
        return;
    }

    if (view.header.command == GUIWIRE_CMD_SET_BAUD) {
        uint32_t requested_baud;
        uint8_t response[13];
        uint8_t offset = runtime->reliable_session ? GUIWIRE_SESSION_TOKEN_SIZE : 0u;

        if (view.header.payload_len == 13u &&
            !runtime_session_matches(runtime, view.payload)) {
            runtime_session_mismatch(runtime, view.payload);
            return;
        }
        if (!guiwire_device_session_active(&runtime->device) ||
            view.header.payload_len != (uint16_t)(offset + 5u) || !runtime->io.set_baud ||
            !runtime->io.supported_bauds || !runtime->io.supported_baud_count) return;
        runtime_note_authenticated_host(runtime);
        requested_baud = guiwire_read_u32le(&view.payload[offset]);
        if (!runtime_baud_supported(runtime, requested_baud)) return;

        if (offset) memcpy(response, runtime->session_token, offset);
        guiwire_write_u32le(&response[offset], requested_baud);
        response[offset + 4u] = view.payload[offset + 4u];
        if (runtime_send_command_payload(runtime, GUIWIRE_CMD_BAUD_ACK,
                                          response, (uint16_t)(offset + 5u)) != GUIWIRE_STATUS_OK) {
            return;
        }
        if (runtime->io.set_baud(runtime->io.set_baud_ctx, requested_baud) != GUIWIRE_STATUS_OK) {
            runtime_restore_base_baud(runtime);
            return;
        }
        runtime->current_baud = requested_baud;
        runtime->baud_fallback_active = true;
        runtime->baud_fallback_deadline_ms = runtime->now_ms +
            (runtime->io.baud_fallback_ms ? runtime->io.baud_fallback_ms : 1000u);
        return;
    }

    if (view.header.command != GUIWIRE_CMD_POLL) return;
    if (view.header.payload_len == 10u) {
        if (!runtime_session_matches(runtime, view.payload)) {
            runtime_session_mismatch(runtime, view.payload);
            return;
        }
        runtime_note_authenticated_host(runtime);
        runtime_reply_to_session_poll(runtime, guiwire_read_u16le(&view.payload[8]));
    } else if (view.header.payload_len == 0u && !runtime->reliable_session &&
               guiwire_device_session_active(&runtime->device)) {
        runtime_note_authenticated_host(runtime);
        runtime_reply_to_poll(runtime);
    }
}

guiwire_status_t guiwire_device_runtime_init(guiwire_device_runtime_t *runtime,
                                             const guiwire_device_io_t *io,
                                             guiwire_device_session_fn on_session_start,
                                             guiwire_device_session_fn on_session_end,
                                             void *session_ctx) {
    if (!runtime || !io || !io->write) return GUIWIRE_STATUS_ERR_INVALID_ARGUMENT;
    memset(runtime, 0, sizeof(*runtime));
    runtime->io = *io;
    runtime->on_session_start = on_session_start;
    runtime->on_session_end = on_session_end;
    runtime->session_ctx = session_ctx;
    runtime->current_baud = runtime->io.base_baud;
    guiwire_device_init(&runtime->device);
    guiwire_link_init(&runtime->link, runtime->io.write, runtime->io.write_ctx,
                      runtime_packet, runtime);
    return GUIWIRE_STATUS_OK;
}

void guiwire_device_runtime_feed_byte(guiwire_device_runtime_t *runtime,
                                      uint8_t byte,
                                      uint32_t now_ms) {
    if (!runtime) return;
    runtime->now_ms = now_ms;
    guiwire_link_feed_byte(&runtime->link, byte);
}

void guiwire_device_runtime_tick(guiwire_device_runtime_t *runtime, uint32_t now_ms) {
    uint32_t timeout;
    if (!runtime) return;
    runtime->now_ms = now_ms;
    if (runtime->baud_fallback_active &&
        deadline_reached(now_ms, runtime->baud_fallback_deadline_ms)) {
        runtime_restore_base_baud(runtime);
    }
    timeout = runtime->io.session_timeout_ms ? runtime->io.session_timeout_ms : 300u;
    if (guiwire_device_session_active(&runtime->device) &&
        now_ms - runtime->last_host_ms >= timeout) {
        runtime_restore_base_baud(runtime);
        guiwire_device_set_session(&runtime->device, false);
        runtime->reliable_session = false;
        runtime->inflight = false;
        if (runtime->on_session_end) runtime->on_session_end(runtime->session_ctx, &runtime->device);
    }
}

guiwire_device_t *guiwire_device_runtime_device(guiwire_device_runtime_t *runtime) {
    return runtime ? &runtime->device : NULL;
}

bool guiwire_device_runtime_connected(const guiwire_device_runtime_t *runtime) {
    return runtime && guiwire_device_session_active(&runtime->device);
}
