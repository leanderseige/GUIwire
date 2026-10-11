/*
 * GUIwire host/device request-response loopback test.
 *
 * This deliberately models a half-duplex wire: a host transmission is
 * delivered to the device only after the host has finished its frame, and a
 * device response is delivered back only after the device has finished its
 * frame. There is no local echo in this model; local echo is a transceiver
 * concern and must not enter either protocol parser.
 * Compile this test with GUIWIRE_DEVICE_QUEUE_CAPACITY=10 and
 * GUIWIRE_MAX_PACKET_SIZE=96, matching the ATmega demonstrator.
 */

#include <stdio.h>
#include <string.h>

#include "guiwire/guiwire_device_runtime.h"
#include "guiwire/guiwire_host.h"

enum { BUS_CAPACITY = 512 };

typedef struct {
    uint8_t host_tx[BUS_CAPACITY];
    size_t host_tx_len;
    uint8_t device_tx[BUS_CAPACITY];
    size_t device_tx_len;
    guiwire_host_t *host;
    guiwire_device_runtime_t *device;
    uint32_t now_ms;
} loopback_t;

static guiwire_status_t dummy_fill_rect(void *ctx, uint16_t x, uint16_t y,
                                        uint16_t w, uint16_t h,
                                        guiwire_color_t color) {
    (void)ctx; (void)x; (void)y; (void)w; (void)h; (void)color;
    return GUIWIRE_STATUS_OK;
}

static guiwire_status_t dummy_draw_pixel(void *ctx, int x, int y,
                                         guiwire_color_t color) {
    (void)ctx; (void)x; (void)y; (void)color;
    return GUIWIRE_STATUS_OK;
}

static guiwire_status_t dummy_blit(void *ctx, uint16_t x, uint16_t y,
                                   uint16_t w, uint16_t h,
                                   const guiwire_color_t *pixels,
                                   uint16_t stride) {
    (void)ctx; (void)x; (void)y; (void)w; (void)h; (void)pixels; (void)stride;
    return GUIWIRE_STATUS_OK;
}

static guiwire_status_t dummy_text(void *ctx, uint16_t x, uint16_t y,
                                   const char *text, guiwire_color_t fg,
                                   guiwire_color_t bg) {
    (void)ctx; (void)x; (void)y; (void)text; (void)fg; (void)bg;
    return GUIWIRE_STATUS_OK;
}

static guiwire_status_t dummy_flush(void *ctx) {
    (void)ctx;
    return GUIWIRE_STATUS_OK;
}

static const guiwire_renderer_vtable_t dummy_renderer = {
    .fill_rect = dummy_fill_rect,
    .draw_pixel = dummy_draw_pixel,
    .blit_rgb565 = dummy_blit,
    .draw_text = dummy_text,
    .flush = dummy_flush
};

static guiwire_status_t bus_device_write(void *ctx, uint8_t byte) {
    loopback_t *bus = (loopback_t *)ctx;
    if (bus->device_tx_len >= sizeof(bus->device_tx)) return GUIWIRE_STATUS_ERR_NO_SPACE;
    bus->device_tx[bus->device_tx_len++] = byte;
    return GUIWIRE_STATUS_OK;
}

static void bus_device_tx_complete(void *ctx) {
    loopback_t *bus = (loopback_t *)ctx;
    size_t i;
    for (i = 0u; i < bus->device_tx_len; ++i) {
        guiwire_host_feed_byte(bus->host, bus->device_tx[i], bus->now_ms);
    }
    bus->device_tx_len = 0u;
}

static guiwire_status_t bus_host_write(void *ctx, uint8_t byte) {
    loopback_t *bus = (loopback_t *)ctx;
    if (bus->host_tx_len >= sizeof(bus->host_tx)) return GUIWIRE_STATUS_ERR_NO_SPACE;
    bus->host_tx[bus->host_tx_len++] = byte;
    return GUIWIRE_STATUS_OK;
}

static void bus_host_tx_complete(void *ctx) {
    loopback_t *bus = (loopback_t *)ctx;
    size_t i;
    for (i = 0u; i < bus->host_tx_len; ++i) {
        guiwire_device_runtime_feed_byte(bus->device, bus->host_tx[i], bus->now_ms);
    }
    bus->host_tx_len = 0u;
}

static void make_snapshot(void *ctx, guiwire_device_t *device) {
    guiwire_text_desc_t time_text = {
        .object_id = 1u, .x = 16u, .y = 24u, .w = 208u, .h = 36u,
        .fg = 0xFFFFu, .bg = 0u, .zoom = 4u, .text = "00:00.00"
    };
    guiwire_text_desc_t percentage = {
        .x = 8u, .y = 212u, .w = 64u, .h = 10u,
        .fg = 0xFFFFu, .bg = 0u, .zoom = 1u, .text = "A0: 50%"
    };
    guiwire_ring_desc_t ring = {
        .cx = 40u, .cy = 178u, .radius = 30u, .inner_radius = 0u,
        .opening_angle_deg = 0u, .bg = 0u,
        .fg_active = 0xF800u, .fg_inactive = 0x4208u, .value = 32768u
    };
    (void)ctx;
    (void)guiwire_device_destroy_all_objects(device);
    (void)guiwire_device_queue_hello(device, "loopback");
    (void)guiwire_device_create_text(device, &time_text);
    ring.object_id = 2u;
    (void)guiwire_device_create_ring(device, &ring);
    ring.object_id = 3u;
    ring.cx = 120u;
    (void)guiwire_device_create_ring(device, &ring);
    ring.object_id = 4u;
    ring.cx = 200u;
    (void)guiwire_device_create_ring(device, &ring);
    percentage.object_id = 5u;
    (void)guiwire_device_create_text(device, &percentage);
    percentage.object_id = 6u;
    percentage.x = 88u;
    percentage.text = "A1: 50%";
    (void)guiwire_device_create_text(device, &percentage);
    percentage.object_id = 7u;
    percentage.x = 168u;
    percentage.text = "1/100: 50%";
    (void)guiwire_device_create_text(device, &percentage);
}

static void fail(const char *message) {
    fprintf(stderr, "FAIL: %s\n", message);
}

int main(void) {
    loopback_t bus = {0};
    guiwire_host_t host;
    guiwire_device_runtime_t device;
    guiwire_server_config_t server_cfg = {
        .display_size = { .width = 240u, .height = 240u },
        .renderer = &dummy_renderer,
        .renderer_ctx = NULL,
        .server_id = "loopback"
    };
    guiwire_host_io_t host_io = {
        .write = bus_host_write,
        .write_ctx = &bus,
        .tx_complete = bus_host_tx_complete,
        .tx_complete_ctx = &bus,
        .response_timeout_ms = 500u,
        .discovery_interval_ms = 100u,
        .poll_interval_ms = 10u,
        .session_timeout_ms = 500u,
        .max_missed_responses = 3u,
        .base_baud = 9600u,
        .supported_bauds = NULL,
        .supported_baud_count = 0u,
        .baud_guard_ms = 10u,
        .set_baud = NULL,
        .set_baud_ctx = NULL
    };
    guiwire_device_io_t device_io = {
        .write = bus_device_write,
        .write_ctx = &bus,
        .tx_begin = NULL,
        .tx_begin_ctx = NULL,
        .tx_complete = bus_device_tx_complete,
        .tx_complete_ctx = &bus,
        .session_timeout_ms = 300u,
        .echo_guard_ms = 5u,
        .base_baud = 9600u,
        .supported_bauds = NULL,
        .supported_baud_count = 0u,
        .baud_fallback_ms = 0u,
        .set_baud = NULL,
        .set_baud_ctx = NULL
    };
    uint32_t now;

    bus.host = &host;
    bus.device = &device;
    if (guiwire_host_init(&host, &server_cfg, &host_io) != GUIWIRE_STATUS_OK ||
        guiwire_device_runtime_init(&device, &device_io, make_snapshot, NULL, NULL) != GUIWIRE_STATUS_OK) {
        fail("initialization");
        return 1;
    }

    for (now = 0u; now < 2000u; now += 10u) {
        bus.now_ms = now;
        guiwire_device_runtime_tick(&device, now);
        guiwire_host_tick(&host, now);
    }

    if (!guiwire_host_connected(&host) || host.initial_sync ||
        host.session_resets != 0u ||
        host.last_command != GUIWIRE_CMD_NO_DATA ||
        !host.server.text_objects[1].in_use ||
        host.server.text_objects[1].object_id != 1u ||
        !host.server.ring_objects[0].in_use ||
        host.server.ring_objects[0].object_id != 2u ||
        !host.server.ring_objects[1].in_use ||
        host.server.ring_objects[1].object_id != 3u ||
        !host.server.ring_objects[2].in_use ||
        host.server.ring_objects[2].object_id != 4u ||
        !host.server.text_objects[2].in_use ||
        host.server.text_objects[2].object_id != 5u ||
        !host.server.text_objects[3].in_use ||
        host.server.text_objects[3].object_id != 6u ||
        !host.server.text_objects[4].in_use ||
        host.server.text_objects[4].object_id != 7u) {
        fprintf(stderr, "FAIL: connected=%d initial_sync=%d valid=%lu last_cmd=0x%02X resets=%lu\n",
                guiwire_host_connected(&host) ? 1 : 0,
                host.initial_sync ? 1 : 0,
                (unsigned long)host.valid_packets,
                host.last_command,
                (unsigned long)host.session_resets);
        return 1;
    }
    printf("PASS: fixed-rate protocol loopback, seven GUI objects, valid=%lu resets=%lu\n",
           (unsigned long)host.valid_packets,
           (unsigned long)host.session_resets);
    return 0;
}
