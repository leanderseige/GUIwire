/*
 * SPDX-License-Identifier: MIT
 *
 * Small ST7789 SPI renderer for GUIwire Hosts. The renderer deliberately
 * exposes only the operations needed by the portable GUIwire scene engine.
 */

#include <string.h>

#include "guiwire/guiwire_display_st7789.h"

#define ST7789_CMD_SWRESET 0x01u
#define ST7789_CMD_SLPOUT  0x11u
#define ST7789_CMD_NORON   0x13u
#define ST7789_CMD_INVON   0x21u
#define ST7789_CMD_INVOFF  0x20u
#define ST7789_CMD_DISPON  0x29u
#define ST7789_CMD_CASET   0x2Au
#define ST7789_CMD_PASET   0x2Bu
#define ST7789_CMD_RAMWR   0x2Cu
#define ST7789_CMD_MADCTL  0x36u
#define ST7789_CMD_COLMOD  0x3Au
#define ST7789_CMD_PORCTRL 0xB2u
#define ST7789_CMD_GCTRL   0xB7u
#define ST7789_CMD_VCOMS   0xBBu
#define ST7789_CMD_LCMCTRL 0xC0u
#define ST7789_CMD_VDVVRHEN 0xC2u
#define ST7789_CMD_VRHS    0xC3u
#define ST7789_CMD_VDVS    0xC4u
#define ST7789_CMD_PVGAMCTRL 0xE0u
#define ST7789_CMD_NVGAMCTRL 0xE1u

static void st7789_select(guiwire_display_st7789_t *d, bool active) {
    if (d && d->cfg.gpio_write && d->cfg.pin_cs >= 0) {
        d->cfg.gpio_write(d->cfg.user_ctx, d->cfg.pin_cs, !active);
    }
}

static guiwire_status_t st7789_write_raw(guiwire_display_st7789_t *d,
                                          const uint8_t *data,
                                          size_t len) {
    if (!d || !d->cfg.spi_write || (!data && len != 0u)) {
        return GUIWIRE_STATUS_ERR_STATE;
    }
    if (len != 0u && d->cfg.spi_write(d->cfg.user_ctx, data, len) != GUIWIRE_STATUS_OK) {
        return GUIWIRE_STATUS_ERR_TRANSPORT;
    }
    return GUIWIRE_STATUS_OK;
}

static guiwire_status_t st7789_write_cmd(guiwire_display_st7789_t *d, uint8_t cmd) {
    guiwire_status_t st;

    if (!d || !d->cfg.gpio_write) return GUIWIRE_STATUS_ERR_STATE;
    d->cfg.gpio_write(d->cfg.user_ctx, d->cfg.pin_dc, false);
    st7789_select(d, true);
    st = st7789_write_raw(d, &cmd, 1u);
    st7789_select(d, false);
    return st;
}

static guiwire_status_t st7789_write_data(guiwire_display_st7789_t *d,
                                           const uint8_t *data,
                                           size_t len) {
    guiwire_status_t st;

    if (!d || !d->cfg.gpio_write) return GUIWIRE_STATUS_ERR_STATE;
    if (len == 0u) return GUIWIRE_STATUS_OK;
    d->cfg.gpio_write(d->cfg.user_ctx, d->cfg.pin_dc, true);
    st7789_select(d, true);
    st = st7789_write_raw(d, data, len);
    st7789_select(d, false);
    return st;
}

static guiwire_status_t st7789_write_reg(guiwire_display_st7789_t *d,
                                          uint8_t cmd,
                                          const uint8_t *data,
                                          size_t len) {
    guiwire_status_t st = st7789_write_cmd(d, cmd);
    if (st != GUIWIRE_STATUS_OK) return st;
    return st7789_write_data(d, data, len);
}

static void st7789_delay(guiwire_display_st7789_t *d, uint32_t ms) {
    if (d && d->cfg.delay_ms) d->cfg.delay_ms(d->cfg.user_ctx, ms);
}

static uint8_t st7789_madctl(const guiwire_display_st7789_t *d) {
    static const uint8_t rotation_bits[4] = {0x00u, 0x60u, 0xC0u, 0xA0u};
    uint8_t value = rotation_bits[d->cfg.rotation & 0x03u];
    if (d->cfg.bgr) value |= 0x08u;
    return value;
}

static guiwire_status_t st7789_begin_window(guiwire_display_st7789_t *d,
                                             uint16_t x,
                                             uint16_t y,
                                             uint16_t w,
                                             uint16_t h) {
    uint8_t buf[4];
    uint16_t x0;
    uint16_t y0;
    uint16_t x1;
    uint16_t y1;

    if (!d || !d->cfg.gpio_write || w == 0u || h == 0u) {
        return GUIWIRE_STATUS_ERR_INVALID_ARGUMENT;
    }

    x0 = (uint16_t)(x + d->cfg.x_offset);
    y0 = (uint16_t)(y + d->cfg.y_offset);
    x1 = (uint16_t)(x0 + w - 1u);
    y1 = (uint16_t)(y0 + h - 1u);

    st7789_select(d, true);
    d->cfg.gpio_write(d->cfg.user_ctx, d->cfg.pin_dc, false);
    buf[0] = ST7789_CMD_CASET;
    if (st7789_write_raw(d, buf, 1u) != GUIWIRE_STATUS_OK) goto fail;
    buf[0] = (uint8_t)(x0 >> 8); buf[1] = (uint8_t)x0;
    buf[2] = (uint8_t)(x1 >> 8); buf[3] = (uint8_t)x1;
    d->cfg.gpio_write(d->cfg.user_ctx, d->cfg.pin_dc, true);
    if (st7789_write_raw(d, buf, 4u) != GUIWIRE_STATUS_OK) goto fail;

    d->cfg.gpio_write(d->cfg.user_ctx, d->cfg.pin_dc, false);
    buf[0] = ST7789_CMD_PASET;
    if (st7789_write_raw(d, buf, 1u) != GUIWIRE_STATUS_OK) goto fail;
    buf[0] = (uint8_t)(y0 >> 8); buf[1] = (uint8_t)y0;
    buf[2] = (uint8_t)(y1 >> 8); buf[3] = (uint8_t)y1;
    d->cfg.gpio_write(d->cfg.user_ctx, d->cfg.pin_dc, true);
    if (st7789_write_raw(d, buf, 4u) != GUIWIRE_STATUS_OK) goto fail;

    d->cfg.gpio_write(d->cfg.user_ctx, d->cfg.pin_dc, false);
    buf[0] = ST7789_CMD_RAMWR;
    if (st7789_write_raw(d, buf, 1u) != GUIWIRE_STATUS_OK) goto fail;
    d->cfg.gpio_write(d->cfg.user_ctx, d->cfg.pin_dc, true);
    return GUIWIRE_STATUS_OK;

fail:
    st7789_select(d, false);
    return GUIWIRE_STATUS_ERR_TRANSPORT;
}

static guiwire_status_t st7789_fill_rect(void *ctx,
                                          uint16_t x,
                                          uint16_t y,
                                          uint16_t w,
                                          uint16_t h,
                                          guiwire_color_t color) {
    guiwire_display_st7789_t *d = (guiwire_display_st7789_t *)ctx;
    enum { CHUNK_PIXELS = 256 };
    static uint8_t chunk[CHUNK_PIXELS * 2u];
    uint32_t pixels;
    uint8_t hi;
    uint8_t lo;

    if (!d) return GUIWIRE_STATUS_ERR_INVALID_ARGUMENT;
    if (w == 0u || h == 0u || x >= d->cfg.width || y >= d->cfg.height) return GUIWIRE_STATUS_OK;
    if ((uint32_t)x + w > d->cfg.width) w = (uint16_t)(d->cfg.width - x);
    if ((uint32_t)y + h > d->cfg.height) h = (uint16_t)(d->cfg.height - y);
    if (w == 0u || h == 0u) return GUIWIRE_STATUS_OK;

    if (st7789_begin_window(d, x, y, w, h) != GUIWIRE_STATUS_OK) return GUIWIRE_STATUS_ERR_TRANSPORT;
    hi = (uint8_t)(color >> 8);
    lo = (uint8_t)color;
    for (uint16_t i = 0u; i < CHUNK_PIXELS; ++i) {
        chunk[i * 2u] = hi;
        chunk[i * 2u + 1u] = lo;
    }

    pixels = (uint32_t)w * h;
    while (pixels != 0u) {
        uint32_t now = pixels > CHUNK_PIXELS ? CHUNK_PIXELS : pixels;
        if (st7789_write_raw(d, chunk, (size_t)(now * 2u)) != GUIWIRE_STATUS_OK) {
            st7789_select(d, false);
            return GUIWIRE_STATUS_ERR_TRANSPORT;
        }
        pixels -= now;
    }
    st7789_select(d, false);
    return GUIWIRE_STATUS_OK;
}

static guiwire_status_t st7789_draw_pixel(void *ctx, int x, int y, guiwire_color_t color) {
    guiwire_display_st7789_t *d = (guiwire_display_st7789_t *)ctx;
    uint8_t pixel[2] = {(uint8_t)(color >> 8), (uint8_t)color};

    if (!d) return GUIWIRE_STATUS_ERR_INVALID_ARGUMENT;
    if (x < 0 || y < 0 || x >= d->cfg.width || y >= d->cfg.height) return GUIWIRE_STATUS_OK;
    if (st7789_begin_window(d, (uint16_t)x, (uint16_t)y, 1u, 1u) != GUIWIRE_STATUS_OK) {
        return GUIWIRE_STATUS_ERR_TRANSPORT;
    }
    if (st7789_write_raw(d, pixel, sizeof(pixel)) != GUIWIRE_STATUS_OK) {
        st7789_select(d, false);
        return GUIWIRE_STATUS_ERR_TRANSPORT;
    }
    st7789_select(d, false);
    return GUIWIRE_STATUS_OK;
}

static guiwire_status_t st7789_blit_rgb565(void *ctx, uint16_t x, uint16_t y,
                                           uint16_t w, uint16_t h,
                                           const guiwire_color_t *pixels,
                                           uint16_t stride) {
    guiwire_display_st7789_t *d = (guiwire_display_st7789_t *)ctx;
    enum { CHUNK_PIXELS = 256 };
    static uint8_t chunk[CHUNK_PIXELS * 2u];

    if (!d || !pixels || stride < w || w == 0u || h == 0u) {
        return GUIWIRE_STATUS_ERR_INVALID_ARGUMENT;
    }
    if (x >= d->cfg.width || y >= d->cfg.height) return GUIWIRE_STATUS_OK;
    if ((uint32_t)x + w > d->cfg.width) w = (uint16_t)(d->cfg.width - x);
    if ((uint32_t)y + h > d->cfg.height) h = (uint16_t)(d->cfg.height - y);
    if (w == 0u || h == 0u) return GUIWIRE_STATUS_OK;

    if (st7789_begin_window(d, x, y, w, h) != GUIWIRE_STATUS_OK) {
        return GUIWIRE_STATUS_ERR_TRANSPORT;
    }
    for (uint16_t row = 0u; row < h; ++row) {
        uint16_t offset = 0u;
        while (offset < w) {
            uint16_t count = (uint16_t)(w - offset);
            if (count > CHUNK_PIXELS) count = CHUNK_PIXELS;
            for (uint16_t i = 0u; i < count; ++i) {
                guiwire_color_t pixel = pixels[(size_t)row * stride + offset + i];
                chunk[i * 2u] = (uint8_t)(pixel >> 8);
                chunk[i * 2u + 1u] = (uint8_t)pixel;
            }
            if (st7789_write_raw(d, chunk, (size_t)count * 2u) != GUIWIRE_STATUS_OK) {
                st7789_select(d, false);
                return GUIWIRE_STATUS_ERR_TRANSPORT;
            }
            offset = (uint16_t)(offset + count);
        }
    }
    st7789_select(d, false);
    return GUIWIRE_STATUS_OK;
}

static guiwire_status_t st7789_flush(void *ctx) {
    (void)ctx;
    return GUIWIRE_STATUS_OK;
}

const guiwire_renderer_vtable_t guiwire_display_st7789_renderer = {
    .fill_rect = st7789_fill_rect,
    .draw_pixel = st7789_draw_pixel,
    .blit_rgb565 = st7789_blit_rgb565,
    .flush = st7789_flush,
};

guiwire_status_t guiwire_display_st7789_backlight(guiwire_display_st7789_t *display, bool on) {
    if (!display || !display->cfg.gpio_write || display->cfg.pin_bl < 0) return GUIWIRE_STATUS_OK;
    display->cfg.gpio_write(display->cfg.user_ctx, display->cfg.pin_bl, on);
    return GUIWIRE_STATUS_OK;
}

guiwire_status_t guiwire_display_st7789_init(guiwire_display_st7789_t *display,
                                              const guiwire_display_st7789_config_t *cfg) {
    static const uint8_t porctrl[] = {0x0Cu, 0x0Cu, 0x00u, 0x33u, 0x33u};
    static const uint8_t pv_gamma[] = {0xD0u, 0x04u, 0x0Du, 0x11u, 0x13u, 0x2Bu, 0x3Fu, 0x54u, 0x4Cu, 0x18u, 0x0Du, 0x0Bu, 0x1Fu, 0x23u};
    static const uint8_t nv_gamma[] = {0xD0u, 0x04u, 0x0Cu, 0x11u, 0x13u, 0x2Cu, 0x3Fu, 0x44u, 0x51u, 0x2Fu, 0x1Fu, 0x1Fu, 0x20u, 0x23u};
    uint8_t one;

    if (!display || !cfg || !cfg->spi_write || !cfg->gpio_write || !cfg->delay_ms) {
        return GUIWIRE_STATUS_ERR_INVALID_ARGUMENT;
    }
    memset(display, 0, sizeof(*display));
    display->cfg = *cfg;

    if (display->cfg.pin_cs >= 0) display->cfg.gpio_write(display->cfg.user_ctx, display->cfg.pin_cs, true);
    if (display->cfg.pin_dc >= 0) display->cfg.gpio_write(display->cfg.user_ctx, display->cfg.pin_dc, true);
    if (display->cfg.pin_rst >= 0) {
        display->cfg.gpio_write(display->cfg.user_ctx, display->cfg.pin_rst, true);
        st7789_delay(display, 5u);
        display->cfg.gpio_write(display->cfg.user_ctx, display->cfg.pin_rst, false);
        st7789_delay(display, 20u);
        display->cfg.gpio_write(display->cfg.user_ctx, display->cfg.pin_rst, true);
        st7789_delay(display, 120u);
    }

    (void)st7789_write_cmd(display, ST7789_CMD_SWRESET);
    st7789_delay(display, 150u);
    (void)st7789_write_cmd(display, ST7789_CMD_SLPOUT);
    st7789_delay(display, 120u);

    one = 0x55u; (void)st7789_write_reg(display, ST7789_CMD_COLMOD, &one, 1u);
    one = st7789_madctl(display); (void)st7789_write_reg(display, ST7789_CMD_MADCTL, &one, 1u);
    (void)st7789_write_reg(display, ST7789_CMD_PORCTRL, porctrl, sizeof(porctrl));
    one = 0x35u; (void)st7789_write_reg(display, ST7789_CMD_GCTRL, &one, 1u);
    one = 0x19u; (void)st7789_write_reg(display, ST7789_CMD_VCOMS, &one, 1u);
    one = 0x2Cu; (void)st7789_write_reg(display, ST7789_CMD_LCMCTRL, &one, 1u);
    one = 0x01u; (void)st7789_write_reg(display, ST7789_CMD_VDVVRHEN, &one, 1u);
    one = 0x12u; (void)st7789_write_reg(display, ST7789_CMD_VRHS, &one, 1u);
    one = 0x20u; (void)st7789_write_reg(display, ST7789_CMD_VDVS, &one, 1u);
    (void)st7789_write_reg(display, ST7789_CMD_PVGAMCTRL, pv_gamma, sizeof(pv_gamma));
    (void)st7789_write_reg(display, ST7789_CMD_NVGAMCTRL, nv_gamma, sizeof(nv_gamma));
    (void)st7789_write_cmd(display, display->cfg.invert_colors ? ST7789_CMD_INVON : ST7789_CMD_INVOFF);
    (void)st7789_write_cmd(display, ST7789_CMD_NORON);
    (void)st7789_write_cmd(display, ST7789_CMD_DISPON);
    st7789_delay(display, 20u);
    (void)guiwire_display_st7789_backlight(display, true);
    return GUIWIRE_STATUS_OK;
}
