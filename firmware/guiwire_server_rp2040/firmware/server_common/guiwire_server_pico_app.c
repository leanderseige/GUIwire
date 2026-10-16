/* SPDX-License-Identifier: MIT */

#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "pico/rand.h"
#include "hardware/gpio.h"
#include "hardware/sync.h"
#include <inttypes.h>
#include <stdio.h>
#include "guiwire/guiwire_host.h"
#include "guiwire/guiwire_platform_pico.h"
#include "guiwire_server_pico_app.h"

#define GUIWIRE_UART_RX_RING_CAPACITY 1024u
#define GUIWIRE_UART_ECHO_CAPACITY 64u

#ifndef GUIWIRE_UART_BAUD
#define GUIWIRE_UART_BAUD 9600u
#endif

#ifndef GUIWIRE_ENABLE_BAUD_NEGOTIATION
#define GUIWIRE_ENABLE_BAUD_NEGOTIATION 0
#endif

#if GUIWIRE_ENABLE_BAUD_NEGOTIATION
static const uint32_t gui_wire_supported_bauds[] = {
    9600u, 19200u, 38400u, 57600u
};
#endif

typedef struct {
    guiwire_pico_uart_t *uart;
    volatile uint8_t data[GUIWIRE_UART_RX_RING_CAPACITY];
    volatile uint16_t head;
    volatile uint16_t tail;
    uint8_t tx_echo[GUIWIRE_UART_ECHO_CAPACITY];
    uint16_t tx_echo_len;
    uint16_t tx_echo_pos;
    bool tx_capture_open;
    bool tx_echo_active;
} guiwire_uart_service_t;

static guiwire_uart_service_t *g_uart_service;

static void uart_service_core1(void) {
    guiwire_uart_service_t *service = g_uart_service;

    for (;;) {
        while (uart_is_readable(service->uart->uart)) {
            uint8_t byte = uart_getc(service->uart->uart);
            service->uart->rx_captured_bytes++;
            service->uart->rx_last_captured_byte = byte;
            if (byte == GUIWIRE_SYNC_BYTE) service->uart->rx_captured_sync_bytes++;
            uint16_t head = service->head;
            uint16_t next = (uint16_t)((head + 1u) & (GUIWIRE_UART_RX_RING_CAPACITY - 1u));

            if (next == service->tail) {
                service->uart->rx_overflow_bytes++;
                continue;
            }
            service->data[head] = byte;
            __dmb();
            service->head = next;
        }
        tight_loop_contents();
    }
}

static bool uart_service_readable(const guiwire_uart_service_t *service) {
    return service && service->head != service->tail;
}

static uint8_t uart_service_read_byte(guiwire_uart_service_t *service) {
    uint16_t tail = service->tail;
    uint8_t byte = service->data[tail];
    __dmb();
    service->tail = (uint16_t)((tail + 1u) & (GUIWIRE_UART_RX_RING_CAPACITY - 1u));
    return byte;
}

static guiwire_status_t uart_service_write(void *ctx, uint8_t byte) {
    guiwire_uart_service_t *service = (guiwire_uart_service_t *)ctx;
    guiwire_status_t st;

    if (!service || !service->uart) return GUIWIRE_STATUS_ERR_INVALID_ARGUMENT;
    if (!service->tx_capture_open) {
        /* This is a half-duplex wire. Do not receive the server's own
         * transmission through the local transceiver. The old byte-matching
         * filter could consume the common prefix of a real client response. */
        uart_get_hw(service->uart->uart)->cr &= ~UART_UARTCR_RXE_BITS;
        service->tx_echo_len = 0u;
        service->tx_capture_open = true;
    }
    if (service->tx_echo_len >= GUIWIRE_UART_ECHO_CAPACITY) {
        return GUIWIRE_STATUS_ERR_BUFFER_TOO_SMALL;
    }
    st = guiwire_platform_pico_uart_write_byte(service->uart, byte);
    if (st == GUIWIRE_STATUS_OK) service->tx_echo[service->tx_echo_len++] = byte;
    return st;
}

#if defined(GUIWIRE_SERVER_DEBUG_DISPLAY)
#include "lib/renderer/guiwire_renderer_text.h"
#include "lib/server/guiwire_server_objects.h"

/* While the diagnostic view is held, protocol packets must still update the
 * server object model, but they must not touch the physical display.  A
 * no-op renderer gives us that separation without changing the portable
 * server/object code. */
static guiwire_status_t debug_noop_fill_rect(void *ctx, uint16_t x, uint16_t y,
                                             uint16_t w, uint16_t h,
                                             guiwire_color_t color) {
    (void)ctx; (void)x; (void)y; (void)w; (void)h; (void)color;
    return GUIWIRE_STATUS_OK;
}

static guiwire_status_t debug_noop_draw_pixel(void *ctx, int x, int y,
                                              guiwire_color_t color) {
    (void)ctx; (void)x; (void)y; (void)color;
    return GUIWIRE_STATUS_OK;
}

static guiwire_status_t debug_noop_blit(void *ctx, uint16_t x, uint16_t y,
                                        uint16_t w, uint16_t h,
                                        const guiwire_color_t *pixels,
                                        uint16_t stride) {
    (void)ctx; (void)x; (void)y; (void)w; (void)h; (void)pixels; (void)stride;
    return GUIWIRE_STATUS_OK;
}

static guiwire_status_t debug_noop_text(void *ctx, uint16_t x, uint16_t y,
                                        const char *text, guiwire_color_t fg,
                                        guiwire_color_t bg) {
    (void)ctx; (void)x; (void)y; (void)text; (void)fg; (void)bg;
    return GUIWIRE_STATUS_OK;
}

static guiwire_status_t debug_noop_flush(void *ctx) {
    (void)ctx;
    return GUIWIRE_STATUS_OK;
}

static const guiwire_renderer_vtable_t debug_noop_renderer = {
    .fill_rect = debug_noop_fill_rect,
    .draw_pixel = debug_noop_draw_pixel,
    .blit_rgb565 = debug_noop_blit,
    .draw_text = debug_noop_text,
    .flush = debug_noop_flush
};

static const char *debug_waiting_state(const guiwire_host_t *host) {
    if (host->connected) return host->initial_sync ? "SYNC" : "CONNECTED";
    if (host->waiting == GUIWIRE_HOST_WAIT_PING) return "WAIT PING";
    if (host->waiting == GUIWIRE_HOST_WAIT_POLL) return "WAIT POLL";
    if (host->waiting == GUIWIRE_HOST_WAIT_LINK_INFO) return "WAIT RATE";
    if (host->waiting == GUIWIRE_HOST_WAIT_BAUD_ACK) return "WAIT ACK";
    return "DISCOVERY";
}

static const char *debug_rate_state(const guiwire_host_t *host) {
    switch (host->rate_stage) {
        case GUIWIRE_RATE_DISCOVER: return "DISCOVER";
        case GUIWIRE_RATE_READY: return "READY";
        case GUIWIRE_RATE_TRIAL: return "TRIAL";
        case GUIWIRE_RATE_RECOVER: return "RECOVER";
        case GUIWIRE_RATE_VERIFY_BASE: return "VERIFY BASE";
        default: return "FIXED";
    }
}

static void debug_draw_line(guiwire_server_t *server, uint16_t y, const char *text,
                            guiwire_color_t color) {
    const guiwire_renderer_vtable_t *renderer = server->cfg.renderer;
    uint16_t width = server->cfg.display_size.width > 8u
        ? (uint16_t)(server->cfg.display_size.width - 8u) : server->cfg.display_size.width;
    if (!renderer) return;
    (void)guiwire_server_fill_bg(server, 0u, y, server->cfg.display_size.width, 10u,
                                 guiwire_rgb565(0u, 0u, 0u));
    (void)guiwire_renderer_draw_glyph_5x7_clipped(renderer, server->cfg.renderer_ctx,
                                                   8u, y, width, 9u, 8u, y,
                                                   1u, (unsigned char)' ', color);
    for (size_t i = 0u; text && text[i] != '\0'; ++i) {
        uint16_t x = (uint16_t)(8u + i * 6u);
        if (x >= (uint16_t)(8u + width)) break;
        (void)guiwire_renderer_draw_glyph_5x7_clipped(renderer, server->cfg.renderer_ctx,
                                                       8u, y, width, 9u, x, y,
                                                       1u, (unsigned char)text[i], color);
    }
}

static void debug_redraw(guiwire_host_t *host, const guiwire_pico_uart_t *uart,
                         bool *initialized,
                         const guiwire_renderer_vtable_t *display_renderer) {
    char line[64];
    guiwire_server_t *server = &host->server;
    const guiwire_link_t *link = &host->link;
    const guiwire_renderer_vtable_t *saved_renderer = server->cfg.renderer;

    /* Temporarily route only this diagnostic frame to the real display. */
    server->cfg.renderer = display_renderer;

    if (initialized && !*initialized) {
        (void)guiwire_server_fill_bg(server, 0u, 0u,
                                     server->cfg.display_size.width,
                                     server->cfg.display_size.height,
                                     guiwire_rgb565(0u, 0u, 0u));
        *initialized = true;
    }
    /* The heading makes the adaptive image distinguishable from the known-good
     * fixed 9600-baud image during an A/B hardware test. */
#if GUIWIRE_ENABLE_BAUD_NEGOTIATION
    debug_draw_line(server, 8u, "GUIWIRE ADAPTIVE", guiwire_rgb565(0u, 255u, 0u));
#else
    debug_draw_line(server, 8u, "GUIWIRE PINFIX 9600", guiwire_rgb565(0u, 255u, 0u));
#endif

    (void)snprintf(line, sizeof(line), "STATE %s", debug_waiting_state(host));
    debug_draw_line(server, 24u, line, guiwire_rgb565(255u, 255u, 255u));
    (void)snprintf(line, sizeof(line), "TXF %" PRIu32 " RXB %" PRIu32,
                   host->tx_frames, link->rx_bytes);
    debug_draw_line(server, 40u, line, guiwire_rgb565(255u, 255u, 255u));
    (void)snprintf(line, sizeof(line), "RAW %" PRIu32 " S %" PRIu32,
                   uart ? uart->rx_captured_bytes : 0u,
                   uart ? uart->rx_captured_sync_bytes : 0u);
    debug_draw_line(server, 56u, line, guiwire_rgb565(255u, 255u, 255u));
    (void)snprintf(line, sizeof(line), "DROP %" PRIu32 " DS %" PRIu32,
                   uart ? uart->rx_flushed_bytes : 0u,
                   uart ? uart->rx_flushed_sync_bytes : 0u);
    debug_draw_line(server, 72u, line, guiwire_rgb565(255u, 255u, 255u));
    (void)snprintf(line, sizeof(line), "SYNC %" PRIu32 " OVF %" PRIu32,
                   link->sync_bytes,
                   uart ? uart->rx_overflow_bytes : 0u);
    debug_draw_line(server, 88u, line, guiwire_rgb565(255u, 255u, 255u));
    (void)snprintf(line, sizeof(line), "VALID %" PRIu32 " BAD %" PRIu32,
                   host->valid_packets, link->invalid_frames);
    debug_draw_line(server, 104u, line, guiwire_rgb565(255u, 255u, 255u));
    (void)snprintf(line, sizeof(line), "CRC %" PRIu32 " L %02X/%02X",
                   link->crc_errors, link->last_byte,
                   uart ? uart->rx_last_captured_byte : 0u);
    debug_draw_line(server, 120u, line, guiwire_rgb565(255u, 255u, 255u));
    (void)snprintf(line, sizeof(line), "RESET %" PRIu32 " CMD %02X",
                   host->session_resets, host->last_command);
    debug_draw_line(server, 136u, line, guiwire_rgb565(255u, 255u, 0u));
    (void)snprintf(line, sizeof(line), "BAUD %" PRIu32, host->current_baud);
    debug_draw_line(server, 152u, line, guiwire_rgb565(255u, 255u, 0u));
    (void)snprintf(line, sizeof(line), "RATE %s %u/3", debug_rate_state(host),
                   host->trial_good_responses);
    debug_draw_line(server, 168u, line, guiwire_rgb565(255u, 255u, 0u));
    (void)snprintf(line, sizeof(line), "UP %" PRIu32 " DN %" PRIu32 " FAIL %" PRIu32,
                   host->rate_upshifts, host->rate_downshifts, host->rate_failures);
    debug_draw_line(server, 184u, line, guiwire_rgb565(255u, 255u, 0u));
#if GUIWIRE_ENABLE_BAUD_NEGOTIATION
    uint16_t rate_good = 0u;
    uint16_t rate_errors = 0u;
    for (uint8_t i = 0u; i < host->io.supported_baud_count && i < 8u; ++i) {
        if (host->io.supported_bauds[i] == host->current_baud) {
            rate_good = host->rate_history[i].good_responses;
            rate_errors = host->rate_history[i].errors;
            break;
        }
    }
    (void)snprintf(line, sizeof(line), "BEST %" PRIu32 " OK %u",
                   host->best_proven_baud, (unsigned)host->rate_good_streak);
    debug_draw_line(server, 200u, line, guiwire_rgb565(255u, 255u, 0u));
    (void)snprintf(line, sizeof(line), "SEEN %u ERR %u",
                   (unsigned)rate_good, (unsigned)rate_errors);
    debug_draw_line(server, 216u, line, guiwire_rgb565(255u, 255u, 0u));
#endif
    (void)guiwire_server_flush(server);
    server->cfg.renderer = saved_renderer;
}
#endif

static void host_tx_complete(void *ctx) {
    guiwire_uart_service_t *service = (guiwire_uart_service_t *)ctx;
    if (!service || !service->uart || !service->uart->uart) return;
    uart_tx_wait_blocking(service->uart->uart);
    /* The ATmega waits briefly after receiving a request before answering.
     * Re-enable RX before that answer starts. Any byte still in the UART FIFO
     * belongs to the local echo and is discarded while RX is disabled. */
    sleep_ms(1u);
    while (uart_is_readable(service->uart->uart)) {
        (void)uart_getc(service->uart->uart);
        service->uart->rx_flushed_bytes++;
    }
    uart_get_hw(service->uart->uart)->cr |= UART_UARTCR_RXE_BITS;
    service->tx_echo_pos = 0u;
    service->tx_echo_len = 0u;
    service->tx_echo_active = false;
    service->tx_capture_open = false;
}

static void host_new_session_token(void *ctx,
                                   uint8_t token[GUIWIRE_SESSION_TOKEN_SIZE]) {
    uint64_t value = get_rand_64();
    (void)ctx;
    for (uint8_t i = 0u; i < GUIWIRE_SESSION_TOKEN_SIZE; ++i) {
        token[i] = (uint8_t)(value >> (8u * i));
    }
}

static uint32_t host_time_ms(void *ctx) {
    (void)ctx;
    return to_ms_since_boot(get_absolute_time());
}

int guiwire_server_pico_app_run(const guiwire_server_pico_app_config_t *cfg) {
    guiwire_pico_uart_t uart;
    guiwire_host_t host;
    guiwire_server_config_t server_cfg;
    guiwire_host_io_t host_io;
    guiwire_uart_service_t uart_service;
#if defined(GUIWIRE_SERVER_DEBUG_DISPLAY)
    const guiwire_renderer_vtable_t *display_renderer;
    uint32_t next_debug_ms = 0u;
    bool debug_initialized = false;
    bool debug_active = false;
    bool debug_button_mode = false;
    bool debug_button_last_level = true;
    uint32_t debug_button_last_change_ms = 0u;
#endif

    if (!cfg || !cfg->renderer || !cfg->uart) return -1;
    guiwire_platform_pico_uart_init(&uart, cfg->uart, GUIWIRE_UART_BAUD,
                                    cfg->tx_pin, cfg->rx_pin);
    uart_service.uart = &uart;
    uart_service.head = 0u;
    uart_service.tail = 0u;
    uart_service.tx_echo_len = 0u;
    uart_service.tx_echo_pos = 0u;
    uart_service.tx_capture_open = false;
    uart_service.tx_echo_active = false;
    g_uart_service = &uart_service;
    __dmb();
    multicore_launch_core1(uart_service_core1);

    server_cfg.display_size = cfg->display_size;
    server_cfg.renderer = cfg->renderer;
    server_cfg.renderer_ctx = cfg->renderer_ctx;
    server_cfg.server_id = cfg->server_id;

    host_io.write = uart_service_write;
    host_io.write_ctx = &uart_service;
    host_io.tx_complete = host_tx_complete;
    host_io.tx_complete_ctx = &uart_service;
    host_io.response_timeout_ms = (GUIWIRE_UART_BAUD <= 38400u) ? 500u : 120u;
    host_io.discovery_interval_ms = 100u;
    /* The protocol is request/response, so a shorter poll interval reduces
     * latency when a device has several small incremental updates queued. */
    host_io.poll_interval_ms = 10u;
    host_io.session_timeout_ms = 1500u;
    host_io.max_missed_responses = 3u;
    host_io.base_baud = GUIWIRE_UART_BAUD;
    host_io.baud_fallback_ms = 400u;
    host_io.new_session_token = host_new_session_token;
    host_io.session_token_ctx = NULL;
    host_io.require_reliable_session = true;
    host_io.read_time_ms = host_time_ms;
    host_io.time_ctx = NULL;
#if GUIWIRE_ENABLE_BAUD_NEGOTIATION
    host_io.supported_bauds = gui_wire_supported_bauds;
    host_io.supported_baud_count = (uint8_t)(sizeof(gui_wire_supported_bauds) /
                                             sizeof(gui_wire_supported_bauds[0]));
    host_io.baud_guard_ms = 10u;
    host_io.set_baud = guiwire_platform_pico_uart_set_baud;
    host_io.set_baud_ctx = &uart;
#else
    /* Fixed-rate failsafe mode: both endpoints stay at the configured base
     * baud and never exchange or apply SET_BAUD commands. */
    host_io.supported_bauds = NULL;
    host_io.supported_baud_count = 0u;
    /* Keep the request/response turn-around even without baud negotiation.
     * The AVR needs a short interval after TX-complete to discard its local
     * echo before it can receive the next POLL. */
    host_io.baud_guard_ms = 10u;
    host_io.set_baud = NULL;
    host_io.set_baud_ctx = NULL;
#endif

#if defined(GUIWIRE_SERVER_DEBUG_DISPLAY)
    if (cfg->debug_button_pin >= 0) {
        gpio_init((uint)cfg->debug_button_pin);
        gpio_set_dir((uint)cfg->debug_button_pin, GPIO_IN);
        gpio_pull_up((uint)cfg->debug_button_pin);
        debug_button_last_level = gpio_get((uint)cfg->debug_button_pin) != 0u;
        debug_button_mode = !debug_button_last_level;
        debug_active = debug_button_mode;
    }
#endif

    if (guiwire_host_init(&host, &server_cfg, &host_io) != GUIWIRE_STATUS_OK) return -2;
#if defined(GUIWIRE_SERVER_DEBUG_DISPLAY)
    display_renderer = host.server.cfg.renderer;
    if (debug_active) host.server.cfg.renderer = &debug_noop_renderer;
#endif

    for (;;) {
        uint32_t now_ms = to_ms_since_boot(get_absolute_time());
        while (uart_service_readable(&uart_service)) {
            uint8_t byte = uart_service_read_byte(&uart_service);
            guiwire_host_feed_byte(&host, byte, now_ms);
        }
        now_ms = to_ms_since_boot(get_absolute_time());
        guiwire_host_tick(&host, now_ms);
#if defined(GUIWIRE_SERVER_DEBUG_DISPLAY)
        if (cfg->debug_button_pin >= 0) {
            bool level = gpio_get((uint)cfg->debug_button_pin) != 0u;
            if (level != debug_button_last_level &&
                now_ms - debug_button_last_change_ms >= 30u) {
                debug_button_last_level = level;
                debug_button_last_change_ms = now_ms;
                debug_button_mode = !level;
                debug_active = debug_button_mode;
                debug_initialized = false;
                next_debug_ms = now_ms;
                host.server.cfg.renderer = debug_active ? &debug_noop_renderer : display_renderer;
                if (!debug_active) {
                    if (host.connected && !host.initial_sync) {
                        (void)guiwire_server_redraw(&host.server);
                    } else {
                        /* Redraw a neutral status without modifying the
                         * session model or asking the Device for a snapshot. */
                        (void)guiwire_server_fill_bg(&host.server, 0u, 0u,
                                                     host.server.cfg.display_size.width,
                                                     host.server.cfg.display_size.height,
                                                     guiwire_rgb565(0u, 0u, 0u));
                        (void)guiwire_server_show_waiting(&host.server);
                    }
                }
            }
        }
        if (debug_active && (int32_t)(now_ms - next_debug_ms) >= 0) {
            debug_redraw(&host, &uart, &debug_initialized, display_renderer);
            next_debug_ms = now_ms + 500u;
        }
#endif
        tight_loop_contents();
    }
}
