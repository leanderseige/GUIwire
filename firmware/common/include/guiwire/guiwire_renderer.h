/*
 * SPDX-License-Identifier: MIT
 * Authors: any:frequency, seige.digital GbR, Leander Seige (2026)
 */


#ifndef GUIWIRE_RENDERER_H
#define GUIWIRE_RENDERER_H

#include "guiwire/guiwire_types.h"
#include "guiwire/guiwire_status.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct guiwire_renderer_vtable {
    guiwire_status_t (*fill_rect)(void *ctx, uint16_t x, uint16_t y, uint16_t w, uint16_t h, guiwire_color_t color);
    guiwire_status_t (*draw_pixel)(void *ctx, int x, int y, guiwire_color_t color);
    /* Optional fast path for a packed RGB565 bitmap. Pixels are in native
     * guiwire_color_t form and are separated by stride pixels per row. */
    guiwire_status_t (*blit_rgb565)(void *ctx, uint16_t x, uint16_t y,
                                    uint16_t w, uint16_t h,
                                    const guiwire_color_t *pixels,
                                    uint16_t stride);
    guiwire_status_t (*draw_text)(void *ctx, uint16_t x, uint16_t y, const char *text, guiwire_color_t fg, guiwire_color_t bg);
    guiwire_status_t (*flush)(void *ctx);
} guiwire_renderer_vtable_t;

guiwire_color_t guiwire_rgb565(uint8_t r, uint8_t g, uint8_t b);

#ifdef __cplusplus
}
#endif

#endif
