/* SPDX-License-Identifier: MIT */

#include "hardware/gpio.h"
#include "hardware/spi.h"
#include "guiwire/guiwire_display_ssd1306.h"
#include "guiwire/guiwire_platform_pico.h"
#include "guiwire_server_pico_app.h"
#include "board_config.h"

int main(void) {
    guiwire_pico_spi_bus_t spi_bus = { .spi = GUIWIRE_SPI_PORT };
    guiwire_display_ssd1306_t display;
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
    guiwire_server_pico_app_config_t app_cfg;

    spi_init(GUIWIRE_SPI_PORT, GUIWIRE_SPI_BAUDRATE);
    gpio_set_function(GUIWIRE_PIN_OLED_MOSI, GPIO_FUNC_SPI);
    gpio_set_function(GUIWIRE_PIN_OLED_SCK, GPIO_FUNC_SPI);
    gpio_init(GUIWIRE_PIN_OLED_CS); gpio_set_dir(GUIWIRE_PIN_OLED_CS, GPIO_OUT); gpio_put(GUIWIRE_PIN_OLED_CS, 1);
    gpio_init(GUIWIRE_PIN_OLED_DC); gpio_set_dir(GUIWIRE_PIN_OLED_DC, GPIO_OUT);
    gpio_init(GUIWIRE_PIN_OLED_RST); gpio_set_dir(GUIWIRE_PIN_OLED_RST, GPIO_OUT);

    if (guiwire_display_ssd1306_init(&display, &display_cfg) != GUIWIRE_STATUS_OK) return 1;

    app_cfg.display_size.width = GUIWIRE_SSD1306_WIDTH;
    app_cfg.display_size.height = GUIWIRE_SSD1306_HEIGHT;
    app_cfg.renderer = &guiwire_display_ssd1306_renderer;
    app_cfg.renderer_ctx = guiwire_display_ssd1306_context(&display);
    app_cfg.server_id = "GUIwire SSD1306 Host";
    app_cfg.uart = GUIWIRE_UART_PORT;
    app_cfg.tx_pin = GUIWIRE_PIN_GUIWIRE_TX;
    app_cfg.rx_pin = GUIWIRE_PIN_GUIWIRE_RX;
    app_cfg.debug_button_pin = -1;
    return guiwire_server_pico_app_run(&app_cfg);
}
