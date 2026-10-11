/* SPDX-License-Identifier: MIT */

#include "hardware/gpio.h"
#include "hardware/spi.h"
#include "pico/stdlib.h"

#include "guiwire/guiwire_display_ssd1306.h"
#include "guiwire/guiwire_platform_pico.h"
#include "board_config.h"

static guiwire_display_ssd1306_t g_display;

static void draw_demo(uint32_t frame) {
    const guiwire_renderer_vtable_t *renderer = &guiwire_display_ssd1306_renderer;
    uint16_t progress = (uint16_t)(frame % 120u);
    uint16_t marker = (uint16_t)(2u + ((frame / 2u) % 124u));

    (void)renderer->fill_rect(&g_display, 0u, 0u,
                              GUIWIRE_SSD1306_WIDTH, GUIWIRE_SSD1306_HEIGHT, 0x0000u);
    (void)renderer->fill_rect(&g_display, 0u, 0u, 128u, 1u, 0xFFFFu);
    (void)renderer->fill_rect(&g_display, 0u, 31u, 128u, 1u, 0xFFFFu);
    (void)renderer->fill_rect(&g_display, 0u, 0u, 1u, 32u, 0xFFFFu);
    (void)renderer->fill_rect(&g_display, 127u, 0u, 1u, 32u, 0xFFFFu);
    (void)renderer->fill_rect(&g_display, 4u, 7u, 120u, 7u, 0x39E7u);
    (void)renderer->fill_rect(&g_display, 4u, 7u, (uint16_t)(progress + 1u), 7u, 0xFFFFu);
    (void)renderer->fill_rect(&g_display, marker, 19u, 2u, 8u, 0xFFFFu);
    (void)renderer->flush(&g_display);
}

int main(void) {
    guiwire_pico_spi_bus_t spi_bus = { .spi = GUIWIRE_SPI_PORT };
    guiwire_display_ssd1306_config_t display_cfg = {
        .spi_write = guiwire_platform_pico_spi_write,
        .gpio_write = guiwire_platform_pico_gpio_write,
        .delay_ms = guiwire_platform_pico_delay_ms,
        .user_ctx = &spi_bus,
        .pin_cs = GUIWIRE_PIN_OLED_CS,
        .pin_dc = GUIWIRE_PIN_OLED_DC,
        .pin_rst = GUIWIRE_PIN_OLED_RST,
        .pin_bl = GUIWIRE_PIN_OLED_BL
    };

    spi_init(GUIWIRE_SPI_PORT, GUIWIRE_SPI_BAUDRATE);
    gpio_set_function(GUIWIRE_PIN_OLED_MOSI, GPIO_FUNC_SPI);
    gpio_set_function(GUIWIRE_PIN_OLED_SCK, GPIO_FUNC_SPI);
    gpio_init(GUIWIRE_PIN_OLED_CS); gpio_set_dir(GUIWIRE_PIN_OLED_CS, GPIO_OUT); gpio_put(GUIWIRE_PIN_OLED_CS, 1);
    gpio_init(GUIWIRE_PIN_OLED_DC); gpio_set_dir(GUIWIRE_PIN_OLED_DC, GPIO_OUT);
    gpio_init(GUIWIRE_PIN_OLED_RST); gpio_set_dir(GUIWIRE_PIN_OLED_RST, GPIO_OUT);

    if (guiwire_display_ssd1306_init(&g_display, &display_cfg) != GUIWIRE_STATUS_OK) return 1;
    for (uint32_t frame = 0u;; ++frame) {
        draw_demo(frame);
        sleep_ms(40u);
    }
}
