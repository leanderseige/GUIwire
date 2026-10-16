/* SPDX-License-Identifier: MIT */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "guiwire/guiwire_framebuffer.h"

typedef struct {
    unsigned calls;
    bool fail_once;
    uint16_t x, y, width, height;
    guiwire_color_t first;
} capture_t;

static guiwire_status_t capture(void *ctx, uint16_t x, uint16_t y,
                                 uint16_t width, uint16_t height,
                                 const guiwire_color_t *pixels,
                                 uint16_t stride) {
    capture_t *c = ctx;
    c->calls++;
    c->x = x;
    c->y = y;
    c->width = width;
    c->height = height;
    c->first = pixels[0];
    assert(stride == 12u);
    if (c->fail_once) {
        c->fail_once = false;
        return GUIWIRE_STATUS_ERR_TRANSPORT;
    }
    return GUIWIRE_STATUS_OK;
}

int main(void) {
    guiwire_color_t pixels[12u * 10u];
    guiwire_framebuffer_t fb;
    capture_t c = {0};
    memset(pixels, 0, sizeof(pixels));
    assert(guiwire_framebuffer_init(&fb, pixels, 12u, 10u, capture, &c));
    assert(guiwire_framebuffer_renderer.fill_rect(&fb, 2u, 3u, 3u, 2u,
                                                  0x1234u) == GUIWIRE_STATUS_OK);
    assert(guiwire_framebuffer_renderer.draw_pixel(&fb, 4, 4, 0xABCDu)
           == GUIWIRE_STATUS_OK);
    c.fail_once = true;
    assert(guiwire_framebuffer_renderer.flush(&fb) == GUIWIRE_STATUS_ERR_TRANSPORT);
    assert(c.calls == 1u);
    assert(guiwire_framebuffer_renderer.flush(&fb) == GUIWIRE_STATUS_OK);
    assert(c.calls == 2u && c.x == 2u && c.y == 3u &&
           c.width == 3u && c.height == 2u && c.first == 0x1234u);
    assert(guiwire_framebuffer_renderer.flush(&fb) == GUIWIRE_STATUS_OK);
    assert(c.calls == 2u);
    assert(pixels[4u * 12u + 4u] == 0xABCDu);

    guiwire_color_t source[3u * 2u] = {1u, 2u, 3u, 4u, 5u, 6u};
    assert(guiwire_framebuffer_renderer.blit_rgb565(&fb, 10u, 9u, 3u, 2u,
                                                     source, 3u) == GUIWIRE_STATUS_OK);
    assert(pixels[9u * 12u + 10u] == 1u);
    assert(pixels[9u * 12u + 11u] == 2u);
    assert(guiwire_framebuffer_renderer.flush(&fb) == GUIWIRE_STATUS_OK);
    assert(c.calls == 3u && c.x == 10u && c.y == 9u &&
           c.width == 2u && c.height == 1u);
    assert(guiwire_framebuffer_renderer.fill_rect(&fb, 0u, 0u, 12u, 10u,
                                                  0xF800u) == GUIWIRE_STATUS_OK);
    assert(guiwire_framebuffer_renderer.flush(&fb) == GUIWIRE_STATUS_OK);
    assert(c.calls == 5u && c.x == 0u && c.y == 8u &&
           c.width == 12u && c.height == 2u && c.first == 0xF800u);
    puts("guiwire_framebuffer: OK");
    return 0;
}
