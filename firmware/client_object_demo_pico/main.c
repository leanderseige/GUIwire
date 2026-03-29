#include <stdio.h>
#include <stdlib.h>
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

int main(void) {
    gui2c_client_t client;
    gui2c_pico_i2c_controller_t i2c_bus;
    gui2c_transport_t transport;
    gui2c_text_desc_t text;
    unsigned counter = 0;

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

    text.object_id = 10u;
    text.x = 8u;
    text.y = 24u;
    text.w = 240u;
    text.h = 20u;
    text.fg = gui2c_rgb565(255, 255, 255);
    text.bg = gui2c_rgb565(0, 0, 64);
    text.flags = 0u;
    text.zoom = 2u;
    text.text = "Client object mode";
    gui2c_client_create_text(&client, &text);

    gui2c_client_update_text(&client, &text);

    gui2c_rect_desc_t rect = {
        .object_id = 100,
        .x = 100,
        .y = 100,
        .w = 200,
        .h = 200,
        .fill = gui2c_rgb565(0, 255, 255),
        .border = gui2c_rgb565(255, 255, 255),
        .flags = GUI2C_RECT_FLAG_BORDER_ENABLED
    };

    gui2c_client_create_rect(&client, &rect);
    gui2c_client_update_rect(&client, &rect);

    while (1) {
        char line[32];

        snprintf(line, sizeof(line), "Counter %u", counter++);
        text.object_id = 11u;
        text.x = 8u;
        text.y = 52u;
        text.w = 180u;
        text.h = 20u;
        text.fg = gui2c_rgb565(0, 0, 0);
        text.bg = gui2c_rgb565(255, 0, 0);
        text.flags = 0u;
        text.zoom = 2u;
        text.text = line;
        gui2c_client_update_text(&client, &text);

        uint8_t r = (uint8_t)(rand() & 0xFF);
        uint8_t g = (uint8_t)(rand() & 0xFF);
        uint8_t b = (uint8_t)(rand() & 0xFF);
        rect.fill = gui2c_rgb565(r, g, b);
        gui2c_client_update_rect(&client, &rect);

        sleep_ms(GUI2C_CLIENT_SEND_INTERVAL_MS);
    }
}
