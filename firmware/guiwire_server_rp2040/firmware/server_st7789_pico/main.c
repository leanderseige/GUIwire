/* SPDX-License-Identifier: MIT */

#include "hardware/gpio.h"
#include "hardware/spi.h"
#include "guiwire/guiwire_display_st7789.h"
#include "guiwire/guiwire_platform_pico.h"
#include "guiwire_server_pico_app.h"
#include "board_config.h"

static void init_output_pin(int pin, bool initial_level) {
    if (pin < 0) return;
    gpio_init((uint)pin);
    gpio_set_dir((uint)pin, GPIO_OUT);
    gpio_put((uint)pin, initial_level ? 1u : 0u);
}

int main(void) {
    guiwire_pico_spi_bus_t spi_bus = { .spi = GUIWIRE_SPI_PORT };
    guiwire_display_st7789_t display;
    guiwire_display_st7789_config_t display_cfg = {
        .spi_write = guiwire_platform_pico_spi_write,
        .gpio_write = guiwire_platform_pico_gpio_write,
        .delay_ms = guiwire_platform_pico_delay_ms,
        .user_ctx = &spi_bus,
        .width = GUIWIRE_DISPLAY_WIDTH,
        .height = GUIWIRE_DISPLAY_HEIGHT,
        .x_offset = GUIWIRE_DISPLAY_X_OFFSET,
        .y_offset = GUIWIRE_DISPLAY_Y_OFFSET,
        .rotation = GUIWIRE_DISPLAY_ROTATION,
        .invert_colors = GUIWIRE_DISPLAY_INVERT,
        .bgr = GUIWIRE_DISPLAY_BGR,
        .pin_cs = GUIWIRE_PIN_LCD_CS,
        .pin_dc = GUIWIRE_PIN_LCD_DC,
        .pin_rst = GUIWIRE_PIN_LCD_RST,
        .pin_bl = GUIWIRE_PIN_LCD_BL
    };
    guiwire_server_pico_app_config_t app_cfg;

    spi_init(GUIWIRE_SPI_PORT, GUIWIRE_SPI_BAUDRATE);
    spi_set_format(GUIWIRE_SPI_PORT,
                   8u,
                   GUIWIRE_SPI_CPOL ? SPI_CPOL_1 : SPI_CPOL_0,
                   GUIWIRE_SPI_CPHA ? SPI_CPHA_1 : SPI_CPHA_0,
                   SPI_MSB_FIRST);
    gpio_set_function(GUIWIRE_PIN_LCD_MOSI, GPIO_FUNC_SPI);
    gpio_set_function(GUIWIRE_PIN_LCD_SCK, GPIO_FUNC_SPI);
    init_output_pin(GUIWIRE_PIN_LCD_CS, true);
    init_output_pin(GUIWIRE_PIN_LCD_DC, true);
    init_output_pin(GUIWIRE_PIN_LCD_RST, true);
    init_output_pin(GUIWIRE_PIN_LCD_BL, true);

    if (guiwire_display_st7789_init(&display, &display_cfg) != GUIWIRE_STATUS_OK) return 1;
    (void)guiwire_display_st7789_backlight(&display, true);

    app_cfg.display_size.width = GUIWIRE_DISPLAY_WIDTH;
    app_cfg.display_size.height = GUIWIRE_DISPLAY_HEIGHT;
    app_cfg.renderer = &guiwire_display_st7789_renderer;
    app_cfg.renderer_ctx = guiwire_display_st7789_context(&display);
    app_cfg.server_id = "GUIwire ST7789 Host";
    app_cfg.uart = GUIWIRE_UART_PORT;
    app_cfg.tx_pin = GUIWIRE_PIN_GUIWIRE_TX;
    app_cfg.rx_pin = GUIWIRE_PIN_GUIWIRE_RX;
    app_cfg.debug_button_pin = GUIWIRE_PIN_DEBUG_BUTTON;
    return guiwire_server_pico_app_run(&app_cfg);
}
