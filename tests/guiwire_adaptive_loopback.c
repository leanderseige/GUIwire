/* Adaptive half-duplex rate switching with a loss-injecting byte transport.
 * This checks the protocol state machines, not analogue signal quality or
 * the exact UART bit timing of either microcontroller. */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "guiwire/guiwire_device_runtime.h"
#include "guiwire/guiwire_host.h"

enum { BUS_CAPACITY = 512 };

typedef struct {
    uint8_t host_tx[BUS_CAPACITY];
    uint8_t device_tx[BUS_CAPACITY];
    size_t host_len;
    size_t device_len;
    uint32_t host_baud;
    uint32_t device_baud;
    uint32_t now_ms;
    uint32_t block_responses_at_baud;
    uint8_t drop_baud_ack_once;
    uint8_t drop_one_response;
    uint8_t corrupt_one_response;
    uint32_t session_generation;
    guiwire_host_t *host;
    guiwire_device_runtime_t *device;
} bus_t;

static guiwire_status_t dummy_fill(void *ctx, uint16_t x, uint16_t y,
                                   uint16_t w, uint16_t h, guiwire_color_t c) {
    (void)ctx; (void)x; (void)y; (void)w; (void)h; (void)c;
    return GUIWIRE_STATUS_OK;
}

static guiwire_status_t dummy_pixel(void *ctx, int x, int y, guiwire_color_t c) {
    (void)ctx; (void)x; (void)y; (void)c;
    return GUIWIRE_STATUS_OK;
}

static const guiwire_renderer_vtable_t renderer = {
    .fill_rect = dummy_fill, .draw_pixel = dummy_pixel
};

static guiwire_status_t host_write(void *ctx, uint8_t byte) {
    bus_t *bus = (bus_t *)ctx;
    if (bus->host_len == BUS_CAPACITY) return GUIWIRE_STATUS_ERR_NO_SPACE;
    bus->host_tx[bus->host_len++] = byte;
    return GUIWIRE_STATUS_OK;
}

static guiwire_status_t device_write(void *ctx, uint8_t byte) {
    bus_t *bus = (bus_t *)ctx;
    if (bus->device_len == BUS_CAPACITY) return GUIWIRE_STATUS_ERR_NO_SPACE;
    bus->device_tx[bus->device_len++] = byte;
    return GUIWIRE_STATUS_OK;
}

static void host_tx_done(void *ctx) {
    bus_t *bus = (bus_t *)ctx;
    if (bus->host_baud == bus->device_baud) {
        for (size_t i = 0u; i < bus->host_len; ++i) {
            guiwire_device_runtime_feed_byte(bus->device, bus->host_tx[i], bus->now_ms);
        }
    }
    bus->host_len = 0u;
}

static void device_tx_done(void *ctx) {
    bus_t *bus = (bus_t *)ctx;
    bool drop = bus->host_baud != bus->device_baud ||
                bus->device_baud == bus->block_responses_at_baud;
    /* The command byte is at offset 6 in these short, unescaped frames. */
    if (bus->drop_baud_ack_once && bus->device_len > 6u &&
        bus->device_tx[6] == GUIWIRE_CMD_BAUD_ACK) {
        bus->drop_baud_ack_once = 0u;
        drop = true;
    }
    if (bus->drop_one_response && bus->device_baud > 9600u) {
        bus->drop_one_response = 0u;
        drop = true;
    }
    if (bus->corrupt_one_response && bus->device_baud > 9600u &&
        bus->device_len > 6u) {
        bus->corrupt_one_response = 0u;
        bus->device_tx[6] ^= 0x01u;
    }
    if (!drop) {
        for (size_t i = 0u; i < bus->device_len; ++i) {
            guiwire_host_feed_byte(bus->host, bus->device_tx[i], bus->now_ms);
        }
    }
    bus->device_len = 0u;
}

static guiwire_status_t host_set_baud(void *ctx, uint32_t baud) {
    ((bus_t *)ctx)->host_baud = baud;
    return GUIWIRE_STATUS_OK;
}

static guiwire_status_t device_set_baud(void *ctx, uint32_t baud) {
    ((bus_t *)ctx)->device_baud = baud;
    return GUIWIRE_STATUS_OK;
}

static void new_session_token(void *ctx, uint8_t token[GUIWIRE_SESSION_TOKEN_SIZE]) {
    bus_t *bus = (bus_t *)ctx;
    uint32_t generation = ++bus->session_generation;
    for (uint8_t i = 0u; i < GUIWIRE_SESSION_TOKEN_SIZE; ++i) {
        token[i] = (uint8_t)(generation + i * 37u);
    }
}

static void snapshot(void *ctx, guiwire_device_t *device) {
    static const guiwire_text_desc_t time_text = {
        .object_id = 1u, .x = 16u, .y = 24u, .w = 208u, .h = 36u,
        .fg = 0xFFFFu, .bg = 0u, .zoom = 4u, .text = "00:00.00"
    };
    static const guiwire_ring_desc_t ring = {
        .object_id = 2u, .cx = 40u, .cy = 178u, .radius = 30u,
        .fg_active = 0xF800u, .fg_inactive = 0x4208u, .value = 32768u
    };
    (void)ctx;
    (void)guiwire_device_destroy_all_objects(device);
    (void)guiwire_device_queue_hello(device, "adaptive test");
    (void)guiwire_device_create_text(device, &time_text);
    (void)guiwire_device_create_ring(device, &ring);
}

static bool scene_ok(const guiwire_host_t *host) {
    return host->connected && !host->initial_sync &&
           host->server.text_objects[1].in_use &&
           host->server.text_objects[1].object_id == 1u &&
           host->server.ring_objects[0].in_use &&
           host->server.ring_objects[0].object_id == 2u &&
           host->session_resets == 0u;
}

static bool run_case(const char *name, uint8_t scenario) {
    static const uint32_t bauds[] = {9600u, 19200u, 38400u, 57600u};
    bus_t bus = {0};
    guiwire_host_t host;
    guiwire_device_runtime_t device;
    guiwire_server_config_t server_cfg = {
        .display_size = {.width = 240u, .height = 240u},
        .renderer = &renderer, .server_id = "adaptive test"
    };
    guiwire_host_io_t host_io = {
        .write = host_write, .write_ctx = &bus,
        .tx_complete = host_tx_done, .tx_complete_ctx = &bus,
        .response_timeout_ms = 500u, .discovery_interval_ms = 100u,
        .poll_interval_ms = 10u, .session_timeout_ms = 500u,
        .max_missed_responses = 3u, .base_baud = 9600u,
        .supported_bauds = bauds, .supported_baud_count = 4u,
        .baud_guard_ms = 10u, .baud_fallback_ms = 400u,
        .set_baud = host_set_baud, .set_baud_ctx = &bus,
        .new_session_token = new_session_token, .session_token_ctx = &bus,
        .require_reliable_session = true
    };
    guiwire_device_io_t device_io = {
        .write = device_write, .write_ctx = &bus,
        .tx_complete = device_tx_done, .tx_complete_ctx = &bus,
        .session_timeout_ms = 1200u, .echo_guard_ms = 5u,
        .base_baud = 9600u, .supported_bauds = bauds,
        .supported_baud_count = 4u, .baud_fallback_ms = 400u,
        .set_baud = device_set_baud, .set_baud_ctx = &bus
    };
    bool injected = false;
    uint8_t missing_response_injections = 0u;
    uint8_t trial_corruptions = 0u;
    uint32_t next_update_ms = 0u;
    uint16_t last_rendered_value = 32768u;
    unsigned rendered_updates = 0u;
    uint32_t run_until_ms = (scenario == 6u || scenario == 9u) ? 145000u :
                            (scenario == 0u || scenario == 3u || scenario == 5u ||
                             scenario == 7u || scenario == 10u ||
                             scenario == 11u) ? 75000u :
                            scenario == 8u ? 100000u : 18000u;

    bus.host = &host;
    bus.device = &device;
    bus.host_baud = 9600u;
    bus.device_baud = 9600u;
    if (scenario == 1u || scenario == 6u) bus.block_responses_at_baud = 38400u;
    if (scenario == 8u || scenario == 9u) bus.block_responses_at_baud = 57600u;
    if (scenario == 2u) bus.drop_baud_ack_once = 1u;
    if (scenario == 4u) {
        device_io.supported_bauds = NULL;
        device_io.supported_baud_count = 0u;
        device_io.set_baud = NULL;
        device_io.session_timeout_ms = 300u;
    }
    if (guiwire_host_init(&host, &server_cfg, &host_io) != GUIWIRE_STATUS_OK ||
        guiwire_device_runtime_init(&device, &device_io, snapshot, NULL, NULL) != GUIWIRE_STATUS_OK) {
        fprintf(stderr, "FAIL %s: init\n", name);
        return false;
    }

    for (uint32_t ms = 0u; ms < run_until_ms; ms += 10u) {
        bus.now_ms = ms;
        if (scenario == 6u && ms >= 15000u) bus.block_responses_at_baud = 0u;
        guiwire_device_runtime_tick(&device, ms);
        /* Keep GUI traffic flowing while capability exchange, rate trials,
         * loss recovery and downshifts run in ordinary poll slots. */
        if (ms >= next_update_ms && guiwire_device_runtime_connected(&device) &&
            guiwire_device_pending(&device.device) == 0u) {
            guiwire_ring_update_t update = {
                .object_id = 2u,
                .value = (uint16_t)(1u + (ms / 10u) % 60000u)
            };
            if (guiwire_device_update_ring(&device.device, &update) != GUIWIRE_STATUS_OK) {
                fprintf(stderr, "FAIL %s: queue update\n", name);
                return false;
            }
            next_update_ms = ms + 20u;
        }
        guiwire_host_tick(&host, ms);
        if (host.server.ring_objects[0].in_use &&
            host.server.ring_objects[0].value != last_rendered_value) {
            last_rendered_value = host.server.ring_objects[0].value;
            rendered_updates++;
        }
        if (scenario == 3u && !injected && host.current_baud == 57600u &&
            host.rate_stage == GUIWIRE_RATE_READY) {
            bus.drop_one_response = 1u;
            injected = true;
        }
        if (scenario == 5u && !injected && host.current_baud == 57600u &&
            host.rate_stage == GUIWIRE_RATE_READY) {
            bus.corrupt_one_response = 1u;
            injected = true;
        }
        if ((scenario == 7u || scenario == 10u) &&
            missing_response_injections < 2u &&
            host.current_baud == 57600u &&
            host.rate_stage == GUIWIRE_RATE_READY &&
            host.waiting == GUIWIRE_HOST_IDLE &&
            (missing_response_injections == 0u ||
             (host.rate_error_score == 1u && host.missed_responses == 0u))) {
            bus.drop_one_response = 1u;
            if (scenario == 7u && missing_response_injections == 1u) {
                bus.drop_baud_ack_once = 1u;
            }
            missing_response_injections++;
            injected = true;
        }
        if (scenario == 11u && trial_corruptions < 2u &&
            host.current_baud == 57600u &&
            host.rate_stage == GUIWIRE_RATE_TRIAL &&
            host.waiting == GUIWIRE_HOST_IDLE &&
            (trial_corruptions == 0u || host.trial_good_responses >= 1u)) {
            bus.corrupt_one_response = 1u;
            trial_corruptions++;
            injected = true;
        }
        if (scenario == 7u && injected && host.rate_failures &&
            host.current_baud == 9600u &&
            host.rate_stage == GUIWIRE_RATE_VERIFY_BASE) break;
        if (!scene_ok(&host) && ms > 1000u) break;
    }

    if (!scene_ok(&host) || bus.host_baud != bus.device_baud ||
        rendered_updates < 10u ||
        (scenario == 0u && (host.current_baud != 57600u || host.rate_upshifts != 3u)) ||
        (scenario == 1u && (host.current_baud != 19200u || host.rate_failures == 0u)) ||
        (scenario == 2u && (host.current_baud != 9600u || host.rate_failures == 0u)) ||
        (scenario == 3u && (!injected || host.current_baud != 57600u ||
                            host.rate_downshifts != 0u)) ||
        (scenario == 4u && (host.current_baud != 9600u ||
                            host.rate_stage != GUIWIRE_RATE_OFF)) ||
        (scenario == 5u && (!injected || host.current_baud != 57600u ||
                            host.rate_downshifts != 0u ||
                            host.link.crc_errors == 0u)) ||
        (scenario == 6u && (host.current_baud != 57600u ||
                            host.rate_failures == 0u ||
                            host.rate_upshifts < 3u)) ||
        (scenario == 7u && (!injected || host.current_baud != 9600u ||
                            host.rate_failures == 0u ||
                            host.failed_rate != 57600u)) ||
        (scenario == 8u && (host.current_baud != 38400u ||
                            host.rate_failures != 1u ||
                            host.rate_history[3].failed_trials != 1u ||
                            host.rate_history[2].good_responses < 64u)) ||
        (scenario == 9u && (host.current_baud != 38400u ||
                            host.rate_failures != 2u ||
                            host.rate_history[3].failed_trials != 2u ||
                            host.rate_history[3].retry_after_ms < 200000u)) ||
        (scenario == 10u && (!injected || missing_response_injections != 2u ||
                             host.current_baud != 38400u ||
                             host.rate_downshifts != 1u ||
                             host.rate_failures != 0u)) ||
        (scenario == 11u && (!injected || trial_corruptions != 2u ||
                             host.current_baud != 38400u ||
                             host.rate_downshifts != 1u ||
                             host.rate_failures != 0u))) {
        fprintf(stderr,
                "FAIL %s: host=%lu device=%lu stage=%u up=%lu down=%lu fail=%lu reset=%lu updates=%u\n",
                name, (unsigned long)bus.host_baud, (unsigned long)bus.device_baud,
                (unsigned)host.rate_stage, (unsigned long)host.rate_upshifts,
                (unsigned long)host.rate_downshifts, (unsigned long)host.rate_failures,
                (unsigned long)host.session_resets, rendered_updates);
        return false;
    }
    printf("PASS %s: baud=%lu up=%lu down=%lu fail=%lu resets=%lu updates=%u\n",
           name, (unsigned long)host.current_baud,
           (unsigned long)host.rate_upshifts, (unsigned long)host.rate_downshifts,
           (unsigned long)host.rate_failures, (unsigned long)host.session_resets,
           rendered_updates);
    return true;
}

int main(void) {
    bool good = true;
    good &= run_case("upshift", 0u);
    good &= run_case("failed trial", 1u);
    good &= run_case("lost ACK", 2u);
    good &= run_case("single missing response tolerated", 3u);
    good &= run_case("fixed-rate device", 4u);
    good &= run_case("single CRC tolerated", 5u);
    good &= run_case("retry after cooldown", 6u);
    good &= run_case("lost downshift ACK", 7u);
    good &= run_case("failed upper probe held at 38400", 8u);
    good &= run_case("upper probe exponential retry", 9u);
    good &= run_case("intermittent loss downshift", 10u);
    good &= run_case("intermittent trial corruption downshift", 11u);
    return good ? 0 : 1;
}
