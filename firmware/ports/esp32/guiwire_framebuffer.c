/* SPDX-License-Identifier: MIT */
#include <string.h>
#include "guiwire/guiwire_framebuffer.h"

static void mark_dirty(guiwire_framebuffer_t *fb, uint16_t x, uint16_t y,
                       uint16_t width, uint16_t height) {
    for (uint16_t row = y; row < (uint16_t)(y + height); ++row) {
        if (x < fb->dirty_left[row]) fb->dirty_left[row] = x;
        if (x + width > fb->dirty_right[row]) fb->dirty_right[row] = x + width;
    }
}

bool guiwire_framebuffer_init(guiwire_framebuffer_t *fb, guiwire_color_t *pixels,
                              uint16_t width, uint16_t height,
                              guiwire_framebuffer_present_fn present,
                              void *present_ctx) {
    if (!fb || !pixels || !present || !width || !height ||
        height > GUIWIRE_FRAMEBUFFER_MAX_HEIGHT) return false;
    memset(fb, 0, sizeof(*fb));
    fb->pixels = pixels;
    fb->width = width;
    fb->height = height;
    fb->present = present;
    fb->present_ctx = present_ctx;
    for (uint16_t row = 0; row < height; ++row) fb->dirty_left[row] = width;
    return true;
}

static guiwire_status_t fb_fill_rect(void *ctx, uint16_t x, uint16_t y,
                                     uint16_t width, uint16_t height,
                                     guiwire_color_t color) {
    guiwire_framebuffer_t *fb = ctx;
    if (!fb) return GUIWIRE_STATUS_ERR_INVALID_ARGUMENT;
    if (!width || !height || x >= fb->width || y >= fb->height) return GUIWIRE_STATUS_OK;
    if ((uint32_t)x + width > fb->width) width = fb->width - x;
    if ((uint32_t)y + height > fb->height) height = fb->height - y;
    for (uint16_t row = y; row < (uint16_t)(y + height); ++row) {
        guiwire_color_t *dst = fb->pixels + (size_t)row * fb->width + x;
        for (uint16_t col = 0; col < width; ++col) dst[col] = color;
    }
    mark_dirty(fb, x, y, width, height);
    return GUIWIRE_STATUS_OK;
}

static guiwire_status_t fb_draw_pixel(void *ctx, int x, int y,
                                      guiwire_color_t color) {
    guiwire_framebuffer_t *fb = ctx;
    if (!fb) return GUIWIRE_STATUS_ERR_INVALID_ARGUMENT;
    if (x < 0 || y < 0 || x >= fb->width || y >= fb->height) return GUIWIRE_STATUS_OK;
    fb->pixels[(size_t)y * fb->width + x] = color;
    mark_dirty(fb, (uint16_t)x, (uint16_t)y, 1u, 1u);
    return GUIWIRE_STATUS_OK;
}

static guiwire_status_t fb_blit(void *ctx, uint16_t x, uint16_t y,
                                uint16_t width, uint16_t height,
                                const guiwire_color_t *pixels, uint16_t stride) {
    guiwire_framebuffer_t *fb = ctx;
    if (!fb || !pixels || stride < width) return GUIWIRE_STATUS_ERR_INVALID_ARGUMENT;
    if (!width || !height || x >= fb->width || y >= fb->height) return GUIWIRE_STATUS_OK;
    if ((uint32_t)x + width > fb->width) width = fb->width - x;
    if ((uint32_t)y + height > fb->height) height = fb->height - y;
    for (uint16_t row = 0; row < height; ++row) {
        memcpy(fb->pixels + (size_t)(y + row) * fb->width + x,
               pixels + (size_t)row * stride, (size_t)width * sizeof(*pixels));
    }
    mark_dirty(fb, x, y, width, height);
    return GUIWIRE_STATUS_OK;
}

static guiwire_status_t fb_flush(void *ctx) {
    guiwire_framebuffer_t *fb = ctx;
    if (!fb || !fb->present) return GUIWIRE_STATUS_ERR_INVALID_ARGUMENT;
    for (uint16_t y = 0; y < fb->height;) {
        if (fb->dirty_left[y] >= fb->dirty_right[y]) {
            ++y;
            continue;
        }
        uint16_t x0 = fb->dirty_left[y];
        uint16_t x1 = fb->dirty_right[y];
        uint16_t end = y + 1u;
        /* One bounded DMA transfer per band. Unioning only nearby rows keeps
         * small incremental GUIwire updates from becoming full-screen redraws. */
        while (end < fb->height && end - y < 8u &&
               fb->dirty_left[end] < fb->dirty_right[end]) {
            if (fb->dirty_left[end] < x0) x0 = fb->dirty_left[end];
            if (fb->dirty_right[end] > x1) x1 = fb->dirty_right[end];
            ++end;
        }
        guiwire_status_t st = fb->present(fb->present_ctx, x0, y, x1 - x0,
                                          end - y,
                                          fb->pixels + (size_t)y * fb->width + x0,
                                          fb->width);
        if (st != GUIWIRE_STATUS_OK) return st;
        for (uint16_t row = y; row < end; ++row) {
            fb->dirty_left[row] = fb->width;
            fb->dirty_right[row] = 0u;
        }
        y = end;
    }
    return GUIWIRE_STATUS_OK;
}

const guiwire_renderer_vtable_t guiwire_framebuffer_renderer = {
    .fill_rect = fb_fill_rect,
    .draw_pixel = fb_draw_pixel,
    .blit_rgb565 = fb_blit,
    .draw_text = NULL,
    .flush = fb_flush,
};
