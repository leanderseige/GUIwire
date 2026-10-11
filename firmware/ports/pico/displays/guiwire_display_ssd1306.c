/*
 * SPDX-License-Identifier: MIT
 * Authors: any:frequency, seige.digital GbR, Leander Seige (2026)
 */

#include <string.h>

#include "guiwire/guiwire_display_ssd1306.h"

#define SSD1306_COMMAND 0u
#define SSD1306_DATA 1u

static void ssd1306_gpio(guiwire_display_ssd1306_t *display, int pin, bool level) {
    if (display->cfg.gpio_write && pin >= 0) display->cfg.gpio_write(display->cfg.user_ctx, pin, level);
}

static guiwire_status_t ssd1306_write(guiwire_display_ssd1306_t *display, uint8_t mode,
                                      const uint8_t *data, size_t len) {
    if (!display || !display->cfg.spi_write || (!data && len != 0u)) {
        return GUIWIRE_STATUS_ERR_INVALID_ARGUMENT;
    }
    ssd1306_gpio(display, display->cfg.pin_dc, mode == SSD1306_DATA);
    ssd1306_gpio(display, display->cfg.pin_cs, false);
    guiwire_status_t st = display->cfg.spi_write(display->cfg.user_ctx, data, len);
    ssd1306_gpio(display, display->cfg.pin_cs, true);
    return st;
}

static guiwire_status_t ssd1306_command(guiwire_display_ssd1306_t *display, uint8_t command) {
    return ssd1306_write(display, SSD1306_COMMAND, &command, 1u);
}

static guiwire_status_t ssd1306_command2(guiwire_display_ssd1306_t *display, uint8_t command, uint8_t value) {
    uint8_t bytes[2] = { command, value };
    return ssd1306_write(display, SSD1306_COMMAND, bytes, sizeof(bytes));
}

static guiwire_status_t ssd1306_fill_rect(void *ctx, uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                                          guiwire_color_t color) {
    guiwire_display_ssd1306_t *display = (guiwire_display_ssd1306_t *)ctx;
    uint16_t xx, yy;
    bool on = color != 0u;

    if (!display || x >= GUIWIRE_SSD1306_WIDTH || y >= GUIWIRE_SSD1306_HEIGHT) return GUIWIRE_STATUS_OK;
    if ((uint32_t)x + w > GUIWIRE_SSD1306_WIDTH) w = (uint16_t)(GUIWIRE_SSD1306_WIDTH - x);
    if ((uint32_t)y + h > GUIWIRE_SSD1306_HEIGHT) h = (uint16_t)(GUIWIRE_SSD1306_HEIGHT - y);
    for (yy = y; yy < (uint16_t)(y + h); ++yy) {
        for (xx = x; xx < (uint16_t)(x + w); ++xx) {
            size_t index = (size_t)xx + ((size_t)(yy / 8u) * GUIWIRE_SSD1306_WIDTH);
            uint8_t mask = (uint8_t)(1u << (yy & 7u));
            if (on) display->framebuffer[index] |= mask;
            else display->framebuffer[index] &= (uint8_t)~mask;
        }
    }
    return GUIWIRE_STATUS_OK;
}

static guiwire_status_t ssd1306_draw_pixel(void *ctx, int x, int y, guiwire_color_t color) {
    if (x < 0 || y < 0 || x >= (int)GUIWIRE_SSD1306_WIDTH || y >= (int)GUIWIRE_SSD1306_HEIGHT) {
        return GUIWIRE_STATUS_OK;
    }
    return ssd1306_fill_rect(ctx, (uint16_t)x, (uint16_t)y, 1u, 1u, color);
}

static guiwire_status_t ssd1306_flush(void *ctx) {
    guiwire_display_ssd1306_t *display = (guiwire_display_ssd1306_t *)ctx;
    uint8_t pages = (uint8_t)(GUIWIRE_SSD1306_HEIGHT / 8u);
    uint8_t address[3] = { 0x22u, 0u, (uint8_t)(pages - 1u) };
    guiwire_status_t st;

    if (!display) return GUIWIRE_STATUS_ERR_INVALID_ARGUMENT;
    st = ssd1306_write(display, SSD1306_COMMAND, address, sizeof(address));
    if (st != GUIWIRE_STATUS_OK) return st;
    st = ssd1306_command(display, 0x00u);
    if (st != GUIWIRE_STATUS_OK) return st;
    st = ssd1306_command(display, 0x10u);
    if (st != GUIWIRE_STATUS_OK) return st;
    return ssd1306_write(display, SSD1306_DATA, display->framebuffer, sizeof(display->framebuffer));
}

const guiwire_renderer_vtable_t guiwire_display_ssd1306_renderer = {
    .fill_rect = ssd1306_fill_rect,
    .draw_pixel = ssd1306_draw_pixel,
    .draw_text = NULL,
    .flush = ssd1306_flush
};

guiwire_status_t guiwire_display_ssd1306_init(guiwire_display_ssd1306_t *display,
                                              const guiwire_display_ssd1306_config_t *cfg) {
    guiwire_status_t st;
    if (!display || !cfg || !cfg->spi_write || !cfg->gpio_write || !cfg->delay_ms) {
        return GUIWIRE_STATUS_ERR_INVALID_ARGUMENT;
    }
    memset(display, 0, sizeof(*display));
    display->cfg = *cfg;
    ssd1306_gpio(display, display->cfg.pin_cs, true);
    ssd1306_gpio(display, display->cfg.pin_rst, false);
    display->cfg.delay_ms(display->cfg.user_ctx, 10u);
    ssd1306_gpio(display, display->cfg.pin_rst, true);
    display->cfg.delay_ms(display->cfg.user_ctx, 10u);

    st = ssd1306_command(display, 0xAEu); if (st != GUIWIRE_STATUS_OK) return st;
    st = ssd1306_command2(display, 0xD5u, 0x80u); if (st != GUIWIRE_STATUS_OK) return st;
    st = ssd1306_command2(display, 0xA8u, (uint8_t)(GUIWIRE_SSD1306_HEIGHT - 1u)); if (st != GUIWIRE_STATUS_OK) return st;
    st = ssd1306_command2(display, 0xD3u, 0x00u); if (st != GUIWIRE_STATUS_OK) return st;
    st = ssd1306_command(display, 0x40u); if (st != GUIWIRE_STATUS_OK) return st;
    st = ssd1306_command2(display, 0x8Du, 0x14u); if (st != GUIWIRE_STATUS_OK) return st;
    st = ssd1306_command(display, 0xA1u); if (st != GUIWIRE_STATUS_OK) return st;
    st = ssd1306_command(display, 0xC8u); if (st != GUIWIRE_STATUS_OK) return st;
    st = ssd1306_command2(display, 0xDAu, GUIWIRE_SSD1306_HEIGHT == 32u ? 0x02u : 0x12u); if (st != GUIWIRE_STATUS_OK) return st;
    st = ssd1306_command2(display, 0x81u, 0x8Fu); if (st != GUIWIRE_STATUS_OK) return st;
    st = ssd1306_command2(display, 0xD9u, 0xF1u); if (st != GUIWIRE_STATUS_OK) return st;
    st = ssd1306_command2(display, 0xDBu, 0x40u); if (st != GUIWIRE_STATUS_OK) return st;
    st = ssd1306_command(display, 0xA4u); if (st != GUIWIRE_STATUS_OK) return st;
    st = ssd1306_command(display, 0xA6u); if (st != GUIWIRE_STATUS_OK) return st;
    st = ssd1306_command(display, 0xAFu); if (st != GUIWIRE_STATUS_OK) return st;
    return ssd1306_flush(display);
}

guiwire_status_t guiwire_display_ssd1306_backlight(guiwire_display_ssd1306_t *display, bool on) {
    if (!display) return GUIWIRE_STATUS_ERR_INVALID_ARGUMENT;
    ssd1306_gpio(display, display->cfg.pin_bl, on);
    return GUIWIRE_STATUS_OK;
}
