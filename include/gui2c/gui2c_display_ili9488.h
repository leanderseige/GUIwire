#ifndef GUI2C_DISPLAY_ILI9488_H
#define GUI2C_DISPLAY_ILI9488_H

#include "gui2c/gui2c_display.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    gui2c_spi_write_fn spi_write;
    gui2c_gpio_write_fn gpio_write;
    gui2c_delay_ms_fn delay_ms;
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
} gui2c_display_ili9488_config_t;

typedef struct {
    gui2c_display_ili9488_config_t cfg;
} gui2c_display_ili9488_t;

extern const gui2c_renderer_vtable_t gui2c_display_ili9488_renderer;

gui2c_status_t gui2c_display_ili9488_init(gui2c_display_ili9488_t *display,
                                          const gui2c_display_ili9488_config_t *cfg);

gui2c_status_t gui2c_display_ili9488_backlight(gui2c_display_ili9488_t *display, bool on);

static inline void *gui2c_display_ili9488_context(gui2c_display_ili9488_t *display) {
    return (void *)display;
}

#ifdef __cplusplus
}
#endif

#endif
