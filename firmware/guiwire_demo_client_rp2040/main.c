/* SPDX-License-Identifier: MIT */

#include <string.h>

#include "pico/stdlib.h"
#include "guiwire/guiwire_device_runtime.h"
#include "guiwire/guiwire_platform_pico.h"
#include "board_config.h"

static const uint32_t gui_wire_supported_bauds[] = {
    9600u, 19200u, 38400u, 57600u
};

typedef struct {
    guiwire_device_runtime_t runtime;
    guiwire_pico_uart_t uart;
    uint32_t now_ms;
    uint32_t next_update_ms;
} guiwire_demo_context_t;

static guiwire_status_t demo_write_byte(void *ctx, uint8_t byte) {
    return guiwire_platform_pico_uart_write_byte(ctx, byte);
}

static void demo_tx_complete(void *ctx) {
    guiwire_pico_uart_t *uart = (guiwire_pico_uart_t *)ctx;
    if (!uart || !uart->uart) return;
    uart_tx_wait_blocking(uart->uart);
    sleep_ms(1u);
    while (uart_is_readable(uart->uart)) (void)uart_getc(uart->uart);
    uart_get_hw(uart->uart)->cr |= UART_UARTCR_RXE_BITS;
}

static void demo_tx_begin(void *ctx) {
    guiwire_pico_uart_t *uart = (guiwire_pico_uart_t *)ctx;
    if (!uart || !uart->uart) return;
    uart_get_hw(uart->uart)->cr &= ~UART_UARTCR_RXE_BITS;
    /* Let the Host discard its own local echo before our response starts. */
    sleep_ms(2u);
}

static void demo_build_snapshot(void *ctx, guiwire_device_t *device) {
    guiwire_demo_context_t *demo = (guiwire_demo_context_t *)ctx;
    static const guiwire_text_desc_t title = {
        .object_id = 1u, .x = 4u, .y = 4u, .w = 240u, .h = 18u,
        .fg = 0xFFFFu, .bg = 0x0000u, .flags = 0u, .zoom = 2u,
        .text = "GUIwire RP2040"
    };
    static const guiwire_rect_desc_t frame = {
        .object_id = 2u, .x = 4u, .y = 34u, .w = 300u, .h = 50u,
        .fill = 0x0000u, .border = 0x07E0u, .flags = GUIWIRE_RECT_FLAG_BORDER_ENABLED
    };
    static const guiwire_bar_desc_t bar = {
        .object_id = 3u, .x = 10u, .y = 45u, .w = 288u, .h = 28u,
        .fg = 0x001Fu, .bg = 0x0000u, .value = 0u,
        .direction = GUIWIRE_BAR_DIRECTION_LEFT_TO_RIGHT
    };
    static const guiwire_text_desc_t footer = {
        .object_id = 4u, .x = 8u, .y = 100u, .w = 300u, .h = 12u,
        .fg = 0xFFE0u, .bg = 0x0000u, .flags = 0u, .zoom = 1u,
        .text = "hot-patch ready"
    };

    (void)demo;
    (void)guiwire_device_destroy_all_objects(device);
    (void)guiwire_device_queue_hello(device, "GUIwire RP2040 demo client");
    (void)guiwire_device_create_text(device, &title);
    (void)guiwire_device_create_rect(device, &frame);
    (void)guiwire_device_create_bar(device, &bar);
    (void)guiwire_device_create_text(device, &footer);
}

int main(void) {
    guiwire_demo_context_t demo;
    guiwire_device_io_t io;

    memset(&demo, 0, sizeof(demo));
    guiwire_platform_pico_uart_init(&demo.uart, GUIWIRE_UART_PORT, GUIWIRE_UART_BAUD,
                                    GUIWIRE_PIN_GUIWIRE_TX, GUIWIRE_PIN_GUIWIRE_RX);
    io.write = demo_write_byte;
    io.write_ctx = &demo.uart;
    io.tx_begin = demo_tx_begin;
    io.tx_begin_ctx = &demo.uart;
    io.tx_complete = demo_tx_complete;
    io.tx_complete_ctx = &demo.uart;
    io.session_timeout_ms = 2500u;
    io.echo_guard_ms = 5u;
    io.base_baud = GUIWIRE_UART_BAUD;
    io.supported_bauds = gui_wire_supported_bauds;
    io.supported_baud_count = (uint8_t)(sizeof(gui_wire_supported_bauds) /
                                        sizeof(gui_wire_supported_bauds[0]));
    io.baud_fallback_ms = 400u;
    io.set_baud = guiwire_platform_pico_uart_set_baud;
    io.set_baud_ctx = &demo.uart;
    if (guiwire_device_runtime_init(&demo.runtime, &io, demo_build_snapshot, NULL, &demo) != GUIWIRE_STATUS_OK) {
        return 1;
    }

    for (;;) {
        demo.now_ms = to_ms_since_boot(get_absolute_time());
        while (guiwire_platform_pico_uart_readable(&demo.uart)) {
            guiwire_device_runtime_feed_byte(&demo.runtime,
                                              guiwire_platform_pico_uart_read_byte(&demo.uart),
                                              demo.now_ms);
        }
        guiwire_device_runtime_tick(&demo.runtime, demo.now_ms);
        if (guiwire_device_runtime_connected(&demo.runtime) &&
            demo.now_ms >= demo.next_update_ms &&
            guiwire_device_pending(&demo.runtime.device) == 0u) {
            guiwire_bar_update_t update = {
                .object_id = 3u,
                .value = (uint16_t)(demo.now_ms % 1001u)
            };
            (void)guiwire_device_update_bar(&demo.runtime.device, &update);
            demo.next_update_ms = demo.now_ms + 250u;
        }
        tight_loop_contents();
    }
}
