/* SPDX-License-Identifier: MIT */
#ifndef GUIWIRE_FRAMEBUFFER_H
#define GUIWIRE_FRAMEBUFFER_H

#include "guiwire/guiwire_renderer.h"

#define GUIWIRE_FRAMEBUFFER_MAX_HEIGHT 600u

typedef guiwire_status_t (*guiwire_framebuffer_present_fn)(
    void *ctx, uint16_t x, uint16_t y, uint16_t width, uint16_t height,
    const guiwire_color_t *pixels, uint16_t stride);

typedef struct {
    guiwire_color_t *pixels;
    uint16_t width;
    uint16_t height;
    uint16_t dirty_left[GUIWIRE_FRAMEBUFFER_MAX_HEIGHT];
    uint16_t dirty_right[GUIWIRE_FRAMEBUFFER_MAX_HEIGHT]; /* exclusive */
    guiwire_framebuffer_present_fn present;
    void *present_ctx;
} guiwire_framebuffer_t;

bool guiwire_framebuffer_init(guiwire_framebuffer_t *fb, guiwire_color_t *pixels,
                              uint16_t width, uint16_t height,
                              guiwire_framebuffer_present_fn present,
                              void *present_ctx);

extern const guiwire_renderer_vtable_t guiwire_framebuffer_renderer;

#endif
