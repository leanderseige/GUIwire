/*
 * SPDX-License-Identifier: MIT
 * Authors: any:frequency, seige.digital GbR, Leander Seige (2026)
 */

#ifndef GUIWIRE_DISPLAY_ILI9488_H
#define GUIWIRE_DISPLAY_ILI9488_H

#include "guiwire/guiwire_display.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    guiwire_spi_write_fn spi_write;
    guiwire_gpio_write_fn gpio_write;
    guiwire_delay_ms_fn delay_ms;
    void *user_ctx;
    uint16_t width;
    uint16_t height;
    uint16_t phys_width;
    uint16_t phys_height;
    uint8_t rotation;
    bool invert_colors;
    bool bgr;
    int pin_cs;
    int pin_dc;
    int pin_rst;
    int pin_bl;
} guiwire_display_ili9488_config_t;

typedef struct {
    guiwire_display_ili9488_config_t cfg;
} guiwire_display_ili9488_t;

extern const guiwire_renderer_vtable_t guiwire_display_ili9488_renderer;

guiwire_status_t guiwire_display_ili9488_init(guiwire_display_ili9488_t *display,
                                          const guiwire_display_ili9488_config_t *cfg);

guiwire_status_t guiwire_display_ili9488_backlight(guiwire_display_ili9488_t *display, bool on);

static inline void *guiwire_display_ili9488_context(guiwire_display_ili9488_t *display) {
    return (void *)display;
}

#ifdef __cplusplus
}
#endif

#endif
