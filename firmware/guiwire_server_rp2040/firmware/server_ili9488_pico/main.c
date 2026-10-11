/* SPDX-License-Identifier: MIT */

#include "hardware/gpio.h"
#include "hardware/spi.h"
#include "guiwire/guiwire_display_ili9488.h"
#include "guiwire/guiwire_platform_pico.h"
#include "guiwire_server_pico_app.h"
#include "board_config.h"

int main(void) {
    guiwire_pico_spi_bus_t spi_bus = { .spi = GUIWIRE_SPI_PORT };
    guiwire_display_ili9488_t display;
    guiwire_display_ili9488_config_t display_cfg = {
        .spi_write = guiwire_platform_pico_spi_write,
        .gpio_write = guiwire_platform_pico_gpio_write,
        .delay_ms = guiwire_platform_pico_delay_ms,
        .user_ctx = &spi_bus,
        .width = GUIWIRE_DISPLAY_WIDTH,
        .height = GUIWIRE_DISPLAY_HEIGHT,
        .phys_width = GUIWIRE_DISPLAY_WIDTH,
        .phys_height = GUIWIRE_DISPLAY_HEIGHT,
        .rotation = GUIWIRE_DISPLAY_ROTATION,
        .invert_colors = false,
        .bgr = true,
        .pin_cs = GUIWIRE_PIN_LCD_CS,
        .pin_dc = GUIWIRE_PIN_LCD_DC,
        .pin_rst = GUIWIRE_PIN_LCD_RST,
        .pin_bl = GUIWIRE_PIN_LCD_BL
    };
    guiwire_server_pico_app_config_t app_cfg;

    spi_init(GUIWIRE_SPI_PORT, GUIWIRE_SPI_BAUDRATE);
    gpio_set_function(GUIWIRE_PIN_LCD_MOSI, GPIO_FUNC_SPI);
    gpio_set_function(GUIWIRE_PIN_LCD_MISO, GPIO_FUNC_SPI);
    gpio_set_function(GUIWIRE_PIN_LCD_SCK, GPIO_FUNC_SPI);
    gpio_init(GUIWIRE_PIN_LCD_CS); gpio_set_dir(GUIWIRE_PIN_LCD_CS, GPIO_OUT); gpio_put(GUIWIRE_PIN_LCD_CS, 1);
    gpio_init(GUIWIRE_PIN_LCD_DC); gpio_set_dir(GUIWIRE_PIN_LCD_DC, GPIO_OUT);
    gpio_init(GUIWIRE_PIN_LCD_RST); gpio_set_dir(GUIWIRE_PIN_LCD_RST, GPIO_OUT);
    gpio_init(GUIWIRE_PIN_LCD_BL); gpio_set_dir(GUIWIRE_PIN_LCD_BL, GPIO_OUT); gpio_put(GUIWIRE_PIN_LCD_BL, 1);

    if (guiwire_display_ili9488_init(&display, &display_cfg) != GUIWIRE_STATUS_OK) return 1;
    (void)guiwire_display_ili9488_backlight(&display, true);

    app_cfg.display_size.width = GUIWIRE_DISPLAY_WIDTH;
    app_cfg.display_size.height = GUIWIRE_DISPLAY_HEIGHT;
    app_cfg.renderer = &guiwire_display_ili9488_renderer;
    app_cfg.renderer_ctx = guiwire_display_ili9488_context(&display);
    app_cfg.server_id = "GUIwire ILI9488 Host";
    app_cfg.uart = GUIWIRE_UART_PORT;
    app_cfg.tx_pin = GUIWIRE_PIN_GUIWIRE_TX;
    app_cfg.rx_pin = GUIWIRE_PIN_GUIWIRE_RX;
    app_cfg.debug_button_pin = -1;
    return guiwire_server_pico_app_run(&app_cfg);
}
