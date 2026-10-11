/*
 * SPDX-License-Identifier: MIT
 * Authors: any:frequency, seige.digital GbR, Leander Seige (2026)
 */

#ifndef GUIWIRE_DISPLAY_SSD1306_H
#define GUIWIRE_DISPLAY_SSD1306_H

#include "guiwire/guiwire_display.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef GUIWIRE_SSD1306_WIDTH
#define GUIWIRE_SSD1306_WIDTH 128u
#endif

#ifndef GUIWIRE_SSD1306_HEIGHT
#define GUIWIRE_SSD1306_HEIGHT 64u
#endif

typedef struct {
    guiwire_spi_write_fn spi_write;
    guiwire_gpio_write_fn gpio_write;
    guiwire_delay_ms_fn delay_ms;
    void *user_ctx;
    int pin_cs;
    int pin_dc;
    int pin_rst;
    int pin_bl;
} guiwire_display_ssd1306_config_t;

typedef struct {
    guiwire_display_ssd1306_config_t cfg;
    uint8_t framebuffer[GUIWIRE_SSD1306_WIDTH * GUIWIRE_SSD1306_HEIGHT / 8u];
} guiwire_display_ssd1306_t;

extern const guiwire_renderer_vtable_t guiwire_display_ssd1306_renderer;

guiwire_status_t guiwire_display_ssd1306_init(guiwire_display_ssd1306_t *display,
                                              const guiwire_display_ssd1306_config_t *cfg);

guiwire_status_t guiwire_display_ssd1306_backlight(guiwire_display_ssd1306_t *display, bool on);

static inline void *guiwire_display_ssd1306_context(guiwire_display_ssd1306_t *display) {
    return (void *)display;
}

#ifdef __cplusplus
}
#endif

#endif
