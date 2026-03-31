#include <math.h>
#include <stdio.h>

#include "pico/time.h"
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/i2c.h"

#include "gui2c/gui2c.h"
#include "platform/pico/gui2c_platform_pico.h"
#include "board_config.h"

#define BRAIN_DISPLAY_W 320u
#define BRAIN_DISPLAY_H 480u
#define BRAIN_SECTION_COUNT 4u
#define BRAIN_SECTION_H (BRAIN_DISPLAY_H / BRAIN_SECTION_COUNT)
#define BRAIN_WAVEFORM_COUNT 3u

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
    gui2c_text_desc_t title_text;
    gui2c_text_desc_t learn_cycles_desc;
    gui2c_text_desc_t avg_error_desc;
    gui2c_text_update_t learn_cycles_update;
    gui2c_text_update_t avg_error_update;
    gui2c_waveform_desc_t waveforms[BRAIN_WAVEFORM_COUNT];
    gui2c_waveform_shift_update_t waveform_updates[BRAIN_WAVEFORM_COUNT];
    gui2c_color_t waveform_colors[BRAIN_WAVEFORM_COUNT];
    char learn_cycles_text[32];
    char avg_error_text[32];
    uint32_t learn_cycles = 0u;
    double phase = 0.0;
    uint8_t i;

    stdio_init_all();
    sleep_ms(200);
    printf("GUI2C client-brain-module starting\n");

    board_init_i2c_controller();

    i2c_bus.i2c = GUI2C_I2C_PORT;
    i2c_bus.address = GUI2C_I2C_ADDRESS;
    transport.write = gui2c_platform_pico_i2c_write;
    transport.ctx = &i2c_bus;

    if (gui2c_client_init(&client, transport) != GUI2C_STATUS_OK) {
        while (1) sleep_ms(1000);
    }

    sleep_ms(500);
    gui2c_client_destroy_all_objects(&client);

    title_text.object_id = 10u;
    title_text.x = 8u;
    title_text.y = 8u;
    title_text.w = BRAIN_DISPLAY_W - 16u;
    title_text.h = 36u;
    title_text.fg = gui2c_rgb565(255, 255, 255);
    title_text.bg = gui2c_rgb565(0, 0, 0);
    title_text.flags = 0u;
    title_text.zoom = 4u;
    title_text.text = "any:brain";
    gui2c_client_create_text(&client, &title_text);
    client_packet_pause();

    snprintf(learn_cycles_text, sizeof(learn_cycles_text), "learn cycles: %lu", (unsigned long)learn_cycles);
    learn_cycles_desc.object_id = 11u;
    learn_cycles_desc.x = 8u;
    learn_cycles_desc.y = 58u;
    learn_cycles_desc.w = BRAIN_DISPLAY_W - 16u;
    learn_cycles_desc.h = 18u;
    learn_cycles_desc.fg = gui2c_rgb565(180, 220, 255);
    learn_cycles_desc.bg = gui2c_rgb565(0, 0, 0);
    learn_cycles_desc.flags = 0u;
    learn_cycles_desc.zoom = 2u;
    learn_cycles_desc.text = learn_cycles_text;
    gui2c_client_create_text(&client, &learn_cycles_desc);
    client_packet_pause();
    learn_cycles_update.object_id = learn_cycles_desc.object_id;
    learn_cycles_update.fg = learn_cycles_desc.fg;
    learn_cycles_update.bg = learn_cycles_desc.bg;
    learn_cycles_update.flags = learn_cycles_desc.flags;
    learn_cycles_update.zoom = learn_cycles_desc.zoom;
    learn_cycles_update.text = learn_cycles_text;

    snprintf(avg_error_text, sizeof(avg_error_text), "avg. error: %.4f", 0.5000);
    avg_error_desc.object_id = 12u;
    avg_error_desc.x = 8u;
    avg_error_desc.y = 86u;
    avg_error_desc.w = BRAIN_DISPLAY_W - 16u;
    avg_error_desc.h = 18u;
    avg_error_desc.fg = gui2c_rgb565(255, 210, 160);
    avg_error_desc.bg = gui2c_rgb565(0, 0, 0);
    avg_error_desc.flags = 0u;
    avg_error_desc.zoom = 2u;
    avg_error_desc.text = avg_error_text;
    gui2c_client_create_text(&client, &avg_error_desc);
    client_packet_pause();
    avg_error_update.object_id = avg_error_desc.object_id;
    avg_error_update.fg = avg_error_desc.fg;
    avg_error_update.bg = avg_error_desc.bg;
    avg_error_update.flags = avg_error_desc.flags;
    avg_error_update.zoom = avg_error_desc.zoom;
    avg_error_update.text = avg_error_text;

    waveform_colors[0] = gui2c_rgb565(255, 80, 180);
    waveform_colors[1] = gui2c_rgb565(255, 220, 0);
    waveform_colors[2] = gui2c_rgb565(120, 240, 255);

    for (i = 0u; i < BRAIN_WAVEFORM_COUNT; ++i) {
        uint16_t section_top = (uint16_t)((i + 1u) * BRAIN_SECTION_H);

        waveforms[i].object_id = (gui2c_object_id_t)(100u + i);
        waveforms[i].x = 10u;
        waveforms[i].y = (uint16_t)(section_top + 10u);
        waveforms[i].w = 300u;
        waveforms[i].h = 100u;
        waveforms[i].border = gui2c_rgb565(255, 255, 255);
        waveforms[i].bg = gui2c_rgb565(0, 0, 0);
        waveforms[i].flags = GUI2C_WAVEFORM_FLAG_BORDER_ENABLED;
        waveforms[i].wave_count = 1u;
        waveforms[i].wave_colors = &waveform_colors[i];

        gui2c_client_create_waveform(&client, &waveforms[i]);
        client_packet_pause();

        waveform_updates[i].object_id = waveforms[i].object_id;
        waveform_updates[i].wave_index = 0u;
        waveform_updates[i].x_advance = 2u;
        waveform_updates[i].sample = 0u;
    }

    while (1) {
        learn_cycles += 1u;

        snprintf(learn_cycles_text, sizeof(learn_cycles_text), "learn cycles: %lu", (unsigned long)learn_cycles);
        gui2c_client_update_text(&client, &learn_cycles_update);
        client_packet_pause();

        snprintf(avg_error_text,
                 sizeof(avg_error_text),
                 "avg. error: %.4f",
                 0.3000 + 0.2000 * (sin(phase * 0.25) * 0.5 + 0.5));
        gui2c_client_update_text(&client, &avg_error_update);
        client_packet_pause();

        for (i = 0u; i < BRAIN_WAVEFORM_COUNT; ++i) {
            double wave_phase = phase + ((double)i * (M_PI / 2.0));
            waveform_updates[i].sample =
                (uint16_t)lround((sin(wave_phase) * 0.5 + 0.5) * 65535.0);
            gui2c_client_update_waveform_shift(&client, &waveform_updates[i]);
            client_packet_pause();
        }

        phase += 0.10;
        if (phase >= (2.0 * M_PI)) phase -= (2.0 * M_PI);

        sleep_ms(GUI2C_CLIENT_SEND_INTERVAL_MS);
    }
}
