/* SPDX-License-Identifier: MIT */

#ifndef GUIWIRE_DISPLAY_ST7789_H
#define GUIWIRE_DISPLAY_ST7789_H

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
    uint16_t x_offset;
    uint16_t y_offset;
    uint8_t rotation;
    bool invert_colors;
    bool bgr;
    int pin_cs;
    int pin_dc;
    int pin_rst;
    int pin_bl;
} guiwire_display_st7789_config_t;

typedef struct {
    guiwire_display_st7789_config_t cfg;
} guiwire_display_st7789_t;

extern const guiwire_renderer_vtable_t guiwire_display_st7789_renderer;

guiwire_status_t guiwire_display_st7789_init(guiwire_display_st7789_t *display,
                                              const guiwire_display_st7789_config_t *cfg);

guiwire_status_t guiwire_display_st7789_backlight(guiwire_display_st7789_t *display, bool on);

static inline void *guiwire_display_st7789_context(guiwire_display_st7789_t *display) {
    return (void *)display;
}

#ifdef __cplusplus
}
#endif

#endif
