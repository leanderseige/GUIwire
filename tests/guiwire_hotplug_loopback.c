/* Session-aware GUIwire hot-patch and retransmission tests. The transport is
 * byte-exact but does not model analogue levels or real display timing. */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "guiwire/guiwire_device_runtime.h"
#include "guiwire/guiwire_host.h"

enum { BUS_CAPACITY = 512 };

typedef struct bus bus_t;
typedef struct {
    bus_t *bus;
    guiwire_device_runtime_t runtime;
    uint32_t baud;
    const char *name;
    bool has_rect;
} endpoint_t;

struct bus {
    guiwire_host_t host;
    endpoint_t a;
    endpoint_t b;
    endpoint_t *attached;
    uint8_t host_tx[BUS_CAPACITY];
    uint8_t device_tx[BUS_CAPACITY];
    uint8_t last_data_frame[BUS_CAPACITY];
    size_t host_len;
    size_t device_len;
    size_t last_data_len;
    uint32_t now_ms;
    uint32_t host_baud;
    uint32_t token_counter;
    uint32_t physical_fills;
    uint32_t lost_data_count;
    uint32_t corrupted_data_count;
    uint32_t replayed_count;
    bool drop_next_data;
    bool corrupt_next_data;
    bool replay_old_response_once;
};

static guiwire_status_t draw_fill(void *ctx, uint16_t x, uint16_t y,
                                  uint16_t w, uint16_t h, guiwire_color_t color) {
    (void)x; (void)y; (void)w; (void)h; (void)color;
    ((bus_t *)ctx)->physical_fills++;
    return GUIWIRE_STATUS_OK;
}

static guiwire_status_t draw_pixel(void *ctx, int x, int y, guiwire_color_t color) {
    (void)x; (void)y; (void)color;
    ((bus_t *)ctx)->physical_fills++;
    return GUIWIRE_STATUS_OK;
}

static guiwire_status_t noop_fill(void *ctx, uint16_t x, uint16_t y,
                                  uint16_t w, uint16_t h, guiwire_color_t color) {
    (void)ctx; (void)x; (void)y; (void)w; (void)h; (void)color;
    return GUIWIRE_STATUS_OK;
}

static guiwire_status_t noop_pixel(void *ctx, int x, int y, guiwire_color_t color) {
    (void)ctx; (void)x; (void)y; (void)color;
    return GUIWIRE_STATUS_OK;
}

static const guiwire_renderer_vtable_t physical_renderer = {
    .fill_rect = draw_fill, .draw_pixel = draw_pixel
};
static const guiwire_renderer_vtable_t noop_renderer = {
    .fill_rect = noop_fill, .draw_pixel = noop_pixel
};

static guiwire_status_t host_write(void *ctx, uint8_t byte) {
    bus_t *bus = (bus_t *)ctx;
    if (bus->host_len >= BUS_CAPACITY) return GUIWIRE_STATUS_ERR_NO_SPACE;
    bus->host_tx[bus->host_len++] = byte;
    return GUIWIRE_STATUS_OK;
}

static guiwire_status_t device_write(void *ctx, uint8_t byte) {
    endpoint_t *endpoint = (endpoint_t *)ctx;
    bus_t *bus = endpoint->bus;
    if (bus->device_len >= BUS_CAPACITY) return GUIWIRE_STATUS_ERR_NO_SPACE;
    bus->device_tx[bus->device_len++] = byte;
    return GUIWIRE_STATUS_OK;
}

static void host_tx_done(void *ctx) {
    bus_t *bus = (bus_t *)ctx;
    if (bus->attached && bus->host_baud == bus->attached->baud) {
        for (size_t i = 0u; i < bus->host_len; ++i) {
            guiwire_device_runtime_feed_byte(&bus->attached->runtime,
                                              bus->host_tx[i], bus->now_ms);
        }
    }
    bus->host_len = 0u;
}

static void device_tx_done(void *ctx) {
    endpoint_t *endpoint = (endpoint_t *)ctx;
    bus_t *bus = endpoint->bus;
    bool drop = bus->attached != endpoint || bus->host_baud != endpoint->baud;
    if (!drop && bus->replay_old_response_once && bus->last_data_len) {
        bus->replay_old_response_once = false;
        bus->replayed_count++;
        for (size_t i = 0u; i < bus->last_data_len; ++i) {
            guiwire_host_feed_byte(&bus->host, bus->last_data_frame[i], bus->now_ms);
        }
        bus->device_len = 0u;
        return;
    }
    if (bus->drop_next_data && bus->device_len > 6u &&
        bus->device_tx[6] == GUIWIRE_CMD_SESSION_DATA) {
        bus->drop_next_data = false;
        bus->lost_data_count++;
        drop = true;
    }
    if (bus->corrupt_next_data && bus->device_len > 6u &&
        bus->device_tx[6] == GUIWIRE_CMD_SESSION_DATA) {
        bus->corrupt_next_data = false;
        bus->corrupted_data_count++;
        bus->device_tx[bus->device_len - 1u] ^= 0x01u; /* break the CRC */
    }
    if (!drop) {
        if (bus->device_len > 6u && bus->device_tx[6] == GUIWIRE_CMD_SESSION_DATA) {
            memcpy(bus->last_data_frame, bus->device_tx, bus->device_len);
            bus->last_data_len = bus->device_len;
        }
        for (size_t i = 0u; i < bus->device_len; ++i) {
            guiwire_host_feed_byte(&bus->host, bus->device_tx[i], bus->now_ms);
        }
    }
    bus->device_len = 0u;
}

static guiwire_status_t set_host_baud(void *ctx, uint32_t baud) {
    ((bus_t *)ctx)->host_baud = baud;
    return GUIWIRE_STATUS_OK;
}

static guiwire_status_t set_device_baud(void *ctx, uint32_t baud) {
    ((endpoint_t *)ctx)->baud = baud;
    return GUIWIRE_STATUS_OK;
}

static void new_token(void *ctx, uint8_t token[GUIWIRE_SESSION_TOKEN_SIZE]) {
    bus_t *bus = (bus_t *)ctx;
    uint32_t generation = ++bus->token_counter;
    for (uint8_t i = 0u; i < GUIWIRE_SESSION_TOKEN_SIZE; ++i) {
        token[i] = (uint8_t)(generation + i * 23u);
    }
}

static void snapshot(void *ctx, guiwire_device_t *device) {
    endpoint_t *endpoint = (endpoint_t *)ctx;
    guiwire_text_desc_t text = {
        .object_id = 1u, .x = 4u, .y = 4u, .w = 120u, .h = 10u,
        .fg = 0xFFFFu, .bg = 0u, .zoom = 1u, .text = endpoint->name
    };
    guiwire_rect_desc_t rect = {
        .object_id = 2u, .x = 4u, .y = 30u, .w = 20u, .h = 20u,
        .fill = 0xF800u, .border = 0u
    };
    (void)guiwire_device_destroy_all_objects(device);
    (void)guiwire_device_queue_hello(device, endpoint->name);
    (void)guiwire_device_create_text(device, &text);
    if (endpoint->has_rect) (void)guiwire_device_create_rect(device, &rect);
}

static bool initialize(bus_t *bus, bool adaptive) {
    static const uint32_t bauds[] = {9600u, 19200u, 38400u, 57600u};
    guiwire_server_config_t server_cfg = {
        .display_size = {.width = 240u, .height = 240u},
        .renderer = &physical_renderer, .renderer_ctx = bus,
        .server_id = "hotplug test"
    };
    guiwire_host_io_t host_io = {
        .write = host_write, .write_ctx = bus,
        .tx_complete = host_tx_done, .tx_complete_ctx = bus,
        .response_timeout_ms = 500u, .discovery_interval_ms = 100u,
        .poll_interval_ms = 10u, .session_timeout_ms = 1500u,
        .max_missed_responses = 3u, .base_baud = 9600u,
        .supported_bauds = adaptive ? bauds : NULL,
        .supported_baud_count = adaptive ? 4u : 0u,
        .baud_guard_ms = 10u, .baud_fallback_ms = 400u,
        .set_baud = adaptive ? set_host_baud : NULL,
        .set_baud_ctx = bus, .new_session_token = new_token,
        .session_token_ctx = bus, .require_reliable_session = true
    };
    endpoint_t *endpoints[] = {&bus->a, &bus->b};

    bus->host_baud = 9600u;
    bus->a.name = "CLIENT A";
    bus->a.has_rect = true;
    bus->b.name = "CLIENT B";
    for (size_t i = 0u; i < 2u; ++i) {
        endpoint_t *endpoint = endpoints[i];
        guiwire_device_io_t device_io = {
            .write = device_write, .write_ctx = endpoint,
            .tx_complete = device_tx_done, .tx_complete_ctx = endpoint,
            .session_timeout_ms = adaptive ? 2500u : 1500u,
            .echo_guard_ms = 5u, .base_baud = 9600u,
            .supported_bauds = adaptive ? bauds : NULL,
            .supported_baud_count = adaptive ? 4u : 0u,
            .baud_fallback_ms = 400u,
            .set_baud = adaptive ? set_device_baud : NULL,
            .set_baud_ctx = endpoint
        };
        endpoint->bus = bus;
        endpoint->baud = 9600u;
        if (guiwire_device_runtime_init(&endpoint->runtime, &device_io,
                                        snapshot, NULL, endpoint) != GUIWIRE_STATUS_OK) return false;
    }
    return guiwire_host_init(&bus->host, &server_cfg, &host_io) == GUIWIRE_STATUS_OK;
}

static void advance(bus_t *bus, uint32_t until_ms) {
    while (bus->now_ms < until_ms) {
        bus->now_ms += 10u;
        guiwire_device_runtime_tick(&bus->a.runtime, bus->now_ms);
        guiwire_device_runtime_tick(&bus->b.runtime, bus->now_ms);
        guiwire_host_tick(&bus->host, bus->now_ms);
    }
}

static bool scene_is(const bus_t *bus, const char *name, bool rect) {
    return bus->host.connected && !bus->host.initial_sync &&
           bus->host.reliable_session &&
           bus->host.server.text_objects[1].in_use &&
           strcmp(bus->host.server.text_objects[1].text, name) == 0 &&
           bus->host.server.rect_objects[0].in_use == rect;
}

static bool update_text(endpoint_t *endpoint, const char *text) {
    guiwire_text_update_t update = {
        .object_id = 1u, .fg = 0xFFFFu, .bg = 0u,
        .zoom = 1u, .text = text
    };
    return guiwire_device_update_text(&endpoint->runtime.device, &update) == GUIWIRE_STATUS_OK;
}

static bool run_fixed(void) {
    bus_t bus = {0};
    uint32_t before_fills;
    static const char max_text[] =
        "0123456789ABCDEF" "0123456789ABCDEF"
        "0123456789ABCDEF" "0123456789ABCDEF";
    if (!initialize(&bus, false)) return false;
    advance(&bus, 1000u);
    if (bus.host.connected) return false;
    bus.attached = &bus.a; /* both MCUs were already running */
    advance(&bus, 3000u);
    if (!scene_is(&bus, "CLIENT A", true)) return false;

    bus.replay_old_response_once = true;
    advance(&bus, 3200u);
    if (!scene_is(&bus, "CLIENT A", true) || bus.replayed_count != 1u ||
        bus.host.session_resets != 0u) return false;

    bus.drop_next_data = true;
    if (!update_text(&bus.a, "A RETRIED")) return false;
    advance(&bus, 4500u);
    if (!scene_is(&bus, "A RETRIED", true) || bus.lost_data_count != 1u ||
        bus.host.session_resets != 0u || guiwire_device_pending(&bus.a.runtime.device)) return false;

    bus.corrupt_next_data = true;
    if (!update_text(&bus.a, "A CRC RETRY")) return false;
    advance(&bus, 5800u);
    if (!scene_is(&bus, "A CRC RETRY", true) || bus.corrupted_data_count != 1u ||
        bus.host.link.crc_errors == 0u || bus.host.session_resets != 0u) return false;

    {
        guiwire_text_desc_t extra = {
            .object_id = 3u, .x = 4u, .y = 60u, .w = 230u, .h = 10u,
            .fg = 0xFFFFu, .bg = 0u, .zoom = 1u, .text = max_text
        };
        if (guiwire_device_create_text(&bus.a.runtime.device, &extra) != GUIWIRE_STATUS_OK) return false;
        advance(&bus, 6100u);
        if (!bus.host.server.text_objects[2].in_use ||
            strcmp(bus.host.server.text_objects[2].text, max_text) != 0) return false;
    }

    before_fills = bus.physical_fills;
    bus.host.server.cfg.renderer = &noop_renderer; /* held debug button */
    if (!update_text(&bus.a, "A CACHED")) return false;
    advance(&bus, 6600u);
    if (!scene_is(&bus, "A CACHED", true) || bus.physical_fills != before_fills) return false;
    bus.host.server.cfg.renderer = &physical_renderer; /* button released */
    if (guiwire_server_redraw(&bus.host.server) != GUIWIRE_STATUS_OK ||
        bus.physical_fills == before_fills || bus.host.session_resets != 0u) return false;

    /* B is already running and bound to a different session when plugged in. */
    guiwire_device_set_session(&bus.b.runtime.device, true);
    bus.b.runtime.reliable_session = true;
    memset(bus.b.runtime.session_token, 0xB5, GUIWIRE_SESSION_TOKEN_SIZE);
    bus.b.runtime.last_host_ms = bus.now_ms;
    snapshot(&bus.b, &bus.b.runtime.device);
    before_fills = bus.physical_fills;
    bus.host.server.cfg.renderer = &noop_renderer; /* swap while debug is held */
    bus.attached = &bus.b;
    advance(&bus, 8000u);
    if (!scene_is(&bus, "CLIENT B", false) || bus.host.session_resets != 1u ||
        bus.host.server.text_objects[2].in_use ||
        bus.physical_fills != before_fills) return false;
    if (strcmp(bus.host.server.last_client_string, "CLIENT B") != 0) return false;
    bus.host.server.cfg.renderer = &physical_renderer;
    if (guiwire_server_redraw(&bus.host.server) != GUIWIRE_STATUS_OK ||
        bus.physical_fills == before_fills) return false;

    bus.attached = NULL;
    advance(&bus, 10300u);
    if (bus.host.connected || bus.host.server.last_client_string[0] != '\0' ||
        bus.host.server.text_objects[1].in_use ||
        bus.host.server.rect_objects[0].in_use) return false;
    bus.attached = &bus.a;
    advance(&bus, 12300u);
    if (!scene_is(&bus, "CLIENT A", true)) return false;
    printf("PASS fixed: delayed plug, dedup, lost/CRC DATA retry, cached debug, A/B swap, unplug/replug\n");
    return true;
}

static bool run_adaptive_swap(void) {
    bus_t bus = {0};
    if (!initialize(&bus, true)) return false;
    advance(&bus, 1000u);
    bus.attached = &bus.a;
    advance(&bus, 19000u);
    if (!scene_is(&bus, "CLIENT A", true) || bus.host.current_baud != 57600u) return false;
    guiwire_device_set_session(&bus.b.runtime.device, true);
    bus.b.runtime.reliable_session = true;
    memset(bus.b.runtime.session_token, 0xB5, GUIWIRE_SESSION_TOKEN_SIZE);
    bus.b.runtime.last_host_ms = bus.now_ms;
    snapshot(&bus.b, &bus.b.runtime.device);
    bus.attached = &bus.b; /* B is at base rate while A was at 57600 */
    advance(&bus, 21000u); /* no more than 2 s for fallback and new snapshot */
    if (!scene_is(&bus, "CLIENT B", false) || bus.host.session_resets == 0u) return false;
    advance(&bus, 23000u);
    if (!scene_is(&bus, "CLIENT B", false) || bus.host.session_resets == 0u) return false;
    printf("PASS adaptive: high-rate A to already-active base-rate B, clean new scene\n");
    return true;
}

int main(void) {
    if (!run_fixed() || !run_adaptive_swap()) {
        fprintf(stderr, "FAIL hotplug loopback\n");
        return 1;
    }
    return 0;
}
