
#ifndef GUI2C_RENDERER_H
#define GUI2C_RENDERER_H

#include "gui2c/gui2c_types.h"
#include "gui2c/gui2c_status.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct gui2c_renderer_vtable {
    gui2c_status_t (*fill_rect)(void *ctx, uint16_t x, uint16_t y, uint16_t w, uint16_t h, gui2c_color_t color);
    gui2c_status_t (*draw_pixel)(void *ctx, int x, int y, gui2c_color_t color);
    gui2c_status_t (*draw_text)(void *ctx, uint16_t x, uint16_t y, const char *text, gui2c_color_t fg, gui2c_color_t bg);
    gui2c_status_t (*flush)(void *ctx);
} gui2c_renderer_vtable_t;

gui2c_color_t gui2c_rgb565(uint8_t r, uint8_t g, uint8_t b);

#ifdef __cplusplus
}
#endif

#endif
