#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "pico/time.h"

#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/i2c.h"

#include "gui2c/gui2c.h"
#include "platform/pico/gui2c_platform_pico.h"
#include "board_config.h"

static void board_init_i2c_controller(void) {
    i2c_init(GUI2C_I2C_PORT, GUI2C_I2C_BAUDRATE);
    gpio_set_function(GUI2C_PIN_I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(GUI2C_PIN_I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(GUI2C_PIN_I2C_SDA);
    gpio_pull_up(GUI2C_PIN_I2C_SCL);
}

static void client_packet_pause(void) {
    sleep_ms(10);
}

int main(void) {
    gui2c_client_t client;
    gui2c_pico_i2c_controller_t i2c_bus;
    gui2c_transport_t transport;
    gui2c_bar_desc_t bars[4];
    gui2c_bar_update_t bar_updates[4];
    gui2c_ring_desc_t ring;
    gui2c_ring_update_t ring_update;
    gui2c_text_desc_t title_text;
    gui2c_text_desc_t counter_text;
    gui2c_text_update_t counter_text_update;
    gui2c_line_desc_t line_obj;
    gui2c_line_update_t line_update;
    gui2c_rect_update_t rect_update;
    gui2c_waveform_desc_t waveform;
    gui2c_waveform_shift_multi_update_t waveform_shift;
    gui2c_color_t waveform_colors[2];
    uint8_t waveform_indices[2];
    uint16_t waveform_samples[2];
    unsigned counter = 0;
    unsigned waveform_refresh_tick = 0;
    unsigned slow_update_tick = 0;
    double waveform_phase = 0.0;

    stdio_init_all();
    sleep_ms(200);
    printf("GUI2C client starting\n");

    board_init_i2c_controller();

    i2c_bus.i2c = GUI2C_I2C_PORT;
    i2c_bus.address = GUI2C_I2C_ADDRESS;
    transport.write = gui2c_platform_pico_i2c_write;
    transport.ctx = &i2c_bus;

    if (gui2c_client_init(&client, transport) != GUI2C_STATUS_OK) {
        while (1) sleep_ms(1000);
    }

    srand((unsigned)to_us_since_boot(get_absolute_time()));

    sleep_ms(500);
    gui2c_client_destroy_all_objects(&client);

    title_text.object_id = 10u;
    title_text.x = 8u;
    title_text.y = 24u;
    title_text.w = 240u;
    title_text.h = 20u;
    title_text.fg = gui2c_rgb565(255, 255, 255);
    title_text.bg = gui2c_rgb565(0, 0, 64);
    title_text.flags = 0u;
    title_text.zoom = 2u;
    title_text.text = "Client object mode";
    gui2c_client_create_text(&client, &title_text);
    client_packet_pause();

    counter_text.object_id = 11u;
    counter_text.x = 8u;
    counter_text.y = 52u;
    counter_text.w = 180u;
    counter_text.h = 20u;
    counter_text.fg = gui2c_rgb565(255, 255, 255);
    counter_text.bg = gui2c_rgb565(0, 0, 64);
    counter_text.flags = 0u;
    counter_text.zoom = 2u;
    counter_text.text = "Counter 0";
    gui2c_client_create_text(&client, &counter_text);
    counter_text_update.object_id = counter_text.object_id;
    counter_text_update.fg = counter_text.fg;
    counter_text_update.bg = counter_text.bg;
    counter_text_update.flags = counter_text.flags;
    counter_text_update.zoom = counter_text.zoom;
    counter_text_update.text = counter_text.text;
    client_packet_pause();

    gui2c_rect_desc_t rect = {
        .object_id = 100,
        .x = 100,
        .y = 100,
        .w = 20,
        .h = 20,
        .fill = gui2c_rgb565(0, 255, 255),
        .border = gui2c_rgb565(255, 255, 255),
        .flags = GUI2C_RECT_FLAG_BORDER_ENABLED
    };

    gui2c_client_create_rect(&client, &rect);
    client_packet_pause();
    rect_update.object_id = rect.object_id;
    rect_update.fill = rect.fill;
    rect_update.border = rect.border;
    rect_update.flags = rect.flags;

    line_obj.object_id = 200u;
    line_obj.fg = gui2c_rgb565(255, 255, 0);
    line_obj.bg = gui2c_rgb565(0, 0, 0);
    line_obj.thickness = 1u;
    line_obj.x1 = 35u;
    line_obj.y1 = 100u;
    line_obj.x2 = 65u;
    line_obj.y2 = 100u;
    gui2c_client_create_line(&client, &line_obj);
    client_packet_pause();
    line_update.object_id = line_obj.object_id;
    line_update.fg = line_obj.fg;
    line_update.bg = line_obj.bg;

    bars[0].object_id = 210u;
    bars[0].x = 190u;
    bars[0].y = 48u;
    bars[0].w = 18u;
    bars[0].h = 70u;
    bars[0].fg = gui2c_rgb565(255, 120, 0);
    bars[0].bg = gui2c_rgb565(20, 20, 20);
    bars[0].value = 0u;
    bars[0].direction = (uint8_t)GUI2C_BAR_DIRECTION_TOP_TO_BOTTOM;

    bars[1] = bars[0];
    bars[1].object_id = 211u;
    bars[1].x = 214u;
    bars[1].fg = gui2c_rgb565(0, 220, 120);
    bars[1].direction = (uint8_t)GUI2C_BAR_DIRECTION_BOTTOM_TO_TOP;

    bars[2].object_id = 212u;
    bars[2].x = 190u;
    bars[2].y = 130u;
    bars[2].w = 90u;
    bars[2].h = 14u;
    bars[2].fg = gui2c_rgb565(255, 60, 160);
    bars[2].bg = gui2c_rgb565(20, 20, 20);
    bars[2].value = 0u;
    bars[2].direction = (uint8_t)GUI2C_BAR_DIRECTION_LEFT_TO_RIGHT;

    bars[3] = bars[2];
    bars[3].object_id = 213u;
    bars[3].y = 150u;
    bars[3].fg = gui2c_rgb565(100, 180, 255);
    bars[3].direction = (uint8_t)GUI2C_BAR_DIRECTION_RIGHT_TO_LEFT;

    for (unsigned i = 0u; i < 4u; ++i) {
        gui2c_client_create_bar(&client, &bars[i]);
        client_packet_pause();
    }

    ring.object_id = 220u;
    ring.cx = 265u;
    ring.cy = 88u;
    ring.radius = 28u;
    ring.inner_radius = 16u;
    ring.opening_angle_deg = 60u;
    ring.bg = gui2c_rgb565(0, 0, 0);
    ring.fg_active = gui2c_rgb565(255, 220, 0);
    ring.fg_inactive = gui2c_rgb565(50, 50, 50);
    ring.value = 0u;
    gui2c_client_create_ring(&client, &ring);
    client_packet_pause();

    waveform_colors[0] = gui2c_rgb565(160, 255, 255);
    waveform_colors[1] = gui2c_rgb565(255, 255, 0);
    waveform_indices[0] = 0u;
    waveform_indices[1] = 1u;
    waveform.object_id = 300u;
    waveform.x = 10u;
    waveform.y = 330u;
    waveform.w = 300u;
    waveform.h = 100u;
    waveform.border = gui2c_rgb565(255, 255, 255);
    waveform.bg = gui2c_rgb565(0, 0, 0);
    waveform.flags = GUI2C_WAVEFORM_FLAG_BORDER_ENABLED;
    waveform.wave_count = 2u;
    waveform.wave_colors = waveform_colors;
    gui2c_client_create_waveform(&client, &waveform);
    client_packet_pause();

    while (1) {
        char line[32];
        uint16_t sin_sample;
        uint16_t cos_sample;

        if (slow_update_tick == 0u) {
            unsigned i;

            snprintf(line, sizeof(line), "Counter %u", counter++);
            counter_text_update.text = line;
            gui2c_client_update_text(&client, &counter_text_update);
            client_packet_pause();

            {
                uint8_t r = (uint8_t)(rand() & 0xFF);
                uint8_t g = (uint8_t)(rand() & 0xFF);
                uint8_t b = (uint8_t)(rand() & 0xFF);
                rect_update.fill = gui2c_rgb565(r, g, b);
            }
            gui2c_client_update_rect(&client, &rect_update);
            client_packet_pause();

            line_update.fg = gui2c_rgb565((uint8_t)(128 + 127 * sin(waveform_phase)),
                                          (uint8_t)(128 + 127 * sin(waveform_phase + 2.09439510239)),
                                          (uint8_t)(128 + 127 * sin(waveform_phase + 4.18879020479)));
            gui2c_client_update_line(&client, &line_update);
            client_packet_pause();

            for (i = 0u; i < 4u; ++i) {
                double bar_phase = waveform_phase + ((double)i * (M_PI / 2.0));
                uint16_t bar_value = (uint16_t)lround((sin(bar_phase) * 0.5 + 0.5) * 65535.0);
                bar_updates[i].object_id = bars[i].object_id;
                bar_updates[i].value = bar_value;
                gui2c_client_update_bar(&client, &bar_updates[i]);
                client_packet_pause();
            }

            ring_update.object_id = ring.object_id;
            ring_update.value = (uint16_t)lround((sin(waveform_phase * 0.75) * 0.5 + 0.5) * 65535.0);
            gui2c_client_update_ring(&client, &ring_update);
            client_packet_pause();
        }

        sin_sample = (uint16_t)lround((sin(waveform_phase*4) * 0.5 + 0.5) * 65535.0);
        cos_sample = (uint16_t)lround((rand()&0xFFFF)/2+16384);    // (cos(waveform_phase*16) * 0.25 + 0.5) * 65535.0);

        waveform_samples[0] = sin_sample;
        waveform_samples[1] = cos_sample;
        waveform_shift.object_id = waveform.object_id;
        waveform_shift.wave_count = 2u;
        waveform_shift.x_advance = 4u;
        waveform_shift.wave_indices = waveform_indices;
        waveform_shift.samples = waveform_samples;

        if (waveform_refresh_tick == 0u) {
            gui2c_client_create_waveform(&client, &waveform);
            client_packet_pause();
        }

        gui2c_client_update_waveform_shift_multi(&client, &waveform_shift);
        client_packet_pause();

        waveform_phase += 0.12;
        if (waveform_phase >= (2.0 * M_PI)) waveform_phase -= (2.0 * M_PI);
        waveform_refresh_tick = (waveform_refresh_tick + 1u) % 20u;
        slow_update_tick = (slow_update_tick + 1u) % 4u;

        sleep_ms(GUI2C_CLIENT_SEND_INTERVAL_MS);
    }
}
