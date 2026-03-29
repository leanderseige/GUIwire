
#include <string.h>

#include "gui2c/gui2c.h"
#include "lib/renderer/gui2c_renderer_text.h"

#define ILI9488_CMD_SWRESET   0x01
#define ILI9488_CMD_SLPOUT    0x11
#define ILI9488_CMD_DISPON    0x29
#define ILI9488_CMD_CASET     0x2A
#define ILI9488_CMD_PASET     0x2B
#define ILI9488_CMD_RAMWR     0x2C
#define ILI9488_CMD_MADCTL    0x36
#define ILI9488_CMD_COLMOD    0x3A
#define ILI9488_CMD_GAMSET    0x26
#define ILI9488_CMD_INVON     0x21
#define ILI9488_CMD_INVOFF    0x20
#define ILI9488_CMD_PGAMCTRL  0xE0
#define ILI9488_CMD_NGAMCTRL  0xE1

static gui2c_status_t ili9488_select(gui2c_display_ili9488_t *d, bool active) {
    if (d && d->cfg.gpio_write && d->cfg.pin_cs >= 0) {
        d->cfg.gpio_write(d->cfg.user_ctx, d->cfg.pin_cs, !active);
    }
    return GUI2C_STATUS_OK;
}

static gui2c_status_t ili9488_write_cmd(gui2c_display_ili9488_t *d, uint8_t cmd) {
    if (!d || !d->cfg.spi_write || !d->cfg.gpio_write) return GUI2C_STATUS_ERR_STATE;
    d->cfg.gpio_write(d->cfg.user_ctx, d->cfg.pin_dc, false);
    ili9488_select(d, true);
    if (d->cfg.spi_write(d->cfg.user_ctx, &cmd, 1) != GUI2C_STATUS_OK) {
        ili9488_select(d, false);
        return GUI2C_STATUS_ERR_TRANSPORT;
    }
    ili9488_select(d, false);
    return GUI2C_STATUS_OK;
}

static gui2c_status_t ili9488_write_data(gui2c_display_ili9488_t *d, const uint8_t *data, size_t len) {
    if (!d || !d->cfg.spi_write || !d->cfg.gpio_write) return GUI2C_STATUS_ERR_STATE;
    if (!data || !len) return GUI2C_STATUS_OK;
    d->cfg.gpio_write(d->cfg.user_ctx, d->cfg.pin_dc, true);
    ili9488_select(d, true);
    if (d->cfg.spi_write(d->cfg.user_ctx, data, len) != GUI2C_STATUS_OK) {
        ili9488_select(d, false);
        return GUI2C_STATUS_ERR_TRANSPORT;
    }
    ili9488_select(d, false);
    return GUI2C_STATUS_OK;
}

static gui2c_status_t ili9488_write_reg(gui2c_display_ili9488_t *d, uint8_t cmd, const uint8_t *data, size_t len) {
    gui2c_status_t st = ili9488_write_cmd(d, cmd);
    if (st != GUI2C_STATUS_OK) return st;
    return ili9488_write_data(d, data, len);
}

static void ili9488_delay(gui2c_display_ili9488_t *d, uint32_t ms) {
    if (d && d->cfg.delay_ms) d->cfg.delay_ms(d->cfg.user_ctx, ms);
}

static void gui2c_rgb565_to_rgb666(gui2c_color_t c, uint8_t out[3]) {
    out[0] = (uint8_t)((((c >> 11) & 0x1Fu) << 3) & 0xF8u);
    out[1] = (uint8_t)((((c >> 5) & 0x3Fu) << 2) & 0xFCu);
    out[2] = (uint8_t)(((c & 0x1Fu) << 3) & 0xF8u);
}

static uint8_t ili9488_madctl_for_rotation(const gui2c_display_ili9488_t *d) {
    static const uint8_t lut[4] = {0x48, 0x68, 0xC8, 0xA8};
    uint8_t madctl = lut[d->cfg.rotation & 0x03u];
    if (!d->cfg.bgr) madctl &= (uint8_t)~0x08u;
    return madctl;
}

static gui2c_status_t ili9488_set_window(gui2c_display_ili9488_t *d, uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    uint8_t buf[4];
    gui2c_status_t st;

    buf[0] = (uint8_t)(x0 >> 8); buf[1] = (uint8_t)(x0 & 0xFFu);
    buf[2] = (uint8_t)(x1 >> 8); buf[3] = (uint8_t)(x1 & 0xFFu);
    st = ili9488_write_reg(d, ILI9488_CMD_CASET, buf, 4);
    if (st != GUI2C_STATUS_OK) return st;

    buf[0] = (uint8_t)(y0 >> 8); buf[1] = (uint8_t)(y0 & 0xFFu);
    buf[2] = (uint8_t)(y1 >> 8); buf[3] = (uint8_t)(y1 & 0xFFu);
    st = ili9488_write_reg(d, ILI9488_CMD_PASET, buf, 4);
    if (st != GUI2C_STATUS_OK) return st;

    return ili9488_write_cmd(d, ILI9488_CMD_RAMWR);
}

static gui2c_status_t ili9488_push_repeat(gui2c_display_ili9488_t *d, gui2c_color_t color, uint32_t count) {
    uint8_t px[3], chunk[96];
    uint32_t n;
    size_t i;

    gui2c_rgb565_to_rgb666(color, px);
    for (i = 0; i < sizeof(chunk); i += 3) {
        chunk[i + 0] = px[0];
        chunk[i + 1] = px[1];
        chunk[i + 2] = px[2];
    }

    while (count) {
        n = count;
        if (n > sizeof(chunk) / 3u) n = sizeof(chunk) / 3u;
        if (ili9488_write_data(d, chunk, n * 3u) != GUI2C_STATUS_OK) {
            return GUI2C_STATUS_ERR_TRANSPORT;
        }
        count -= n;
    }

    return GUI2C_STATUS_OK;
}

static gui2c_status_t ili9488_fill_rect(void *ctx,
                                        uint16_t x,
                                        uint16_t y,
                                        uint16_t w,
                                        uint16_t h,
                                        gui2c_color_t color) {
    gui2c_display_ili9488_t *d = (gui2c_display_ili9488_t *)ctx;
    uint32_t pixels_remaining;
    uint8_t rgb[3];

    enum { CHUNK_PIXELS = 256 };
    static uint8_t chunk[CHUNK_PIXELS * 3];
    uint32_t i;

    if (!d) return GUI2C_STATUS_ERR_INVALID_ARGUMENT;
    if (w == 0u || h == 0u) return GUI2C_STATUS_OK;

    if (x >= d->cfg.width || y >= d->cfg.height) return GUI2C_STATUS_OK;

    if ((uint32_t)x + (uint32_t)w > d->cfg.width) {
        w = (uint16_t)(d->cfg.width - x);
    }
    if ((uint32_t)y + (uint32_t)h > d->cfg.height) {
        h = (uint16_t)(d->cfg.height - y);
    }

    if (w == 0u || h == 0u) return GUI2C_STATUS_OK;

    if (ili9488_set_window(d, x, y, (uint16_t)(x + w - 1u), (uint16_t)(y + h - 1u)) != GUI2C_STATUS_OK) {
        return GUI2C_STATUS_ERR_TRANSPORT;
    }

    gui2c_rgb565_to_rgb666(color, rgb);

    for (i = 0; i < CHUNK_PIXELS; ++i) {
        chunk[i * 3u + 0u] = rgb[0];
        chunk[i * 3u + 1u] = rgb[1];
        chunk[i * 3u + 2u] = rgb[2];
    }

    pixels_remaining = (uint32_t)w * (uint32_t)h;

    while (pixels_remaining > 0u) {
        uint32_t pixels_now = (pixels_remaining > CHUNK_PIXELS) ? CHUNK_PIXELS : pixels_remaining;
        uint32_t bytes_now = pixels_now * 3u;

        if (ili9488_write_data(d, chunk, bytes_now) != GUI2C_STATUS_OK) {
            return GUI2C_STATUS_ERR_TRANSPORT;
        }

        pixels_remaining -= pixels_now;
    }

    return GUI2C_STATUS_OK;
}

static gui2c_status_t ili9488_draw_pixel(void *ctx, int x, int y, gui2c_color_t color) {
    gui2c_display_ili9488_t *d = (gui2c_display_ili9488_t *)ctx;
    uint8_t px[3];

    if (!d) return GUI2C_STATUS_ERR_INVALID_ARGUMENT;
    if (x < 0 || y < 0 || x >= d->cfg.width || y >= d->cfg.height) return GUI2C_STATUS_OK;

    if (ili9488_set_window(d, (uint16_t)x, (uint16_t)y, (uint16_t)x, (uint16_t)y) != GUI2C_STATUS_OK) {
        return GUI2C_STATUS_ERR_TRANSPORT;
    }

    gui2c_rgb565_to_rgb666(color, px);
    return ili9488_write_data(d, px, sizeof(px));
}

// static gui2c_status_t ili9488_draw_text(void *ctx,
//                                         uint16_t x,
//                                         uint16_t y,
//                                         const char *text,
//                                         gui2c_color_t fg,
//                                         gui2c_color_t bg) {
//     gui2c_display_ili9488_t *d = (gui2c_display_ili9488_t *)ctx;
//     size_t i, len;
//     gui2c_status_t st;

//     if (!d || !text) return GUI2C_STATUS_ERR_INVALID_ARGUMENT;

//     len = strlen(text);
//     for (i = 0; i < len; ++i) {
//         uint16_t base_x = (uint16_t)(x + i * 6u);

//         st = ili9488_fill_rect(ctx, base_x, y, 6u, 8u, bg);
//         if (st != GUI2C_STATUS_OK) return st;

//         st = gui2c_renderer_draw_glyph_5x7_clipped(&gui2c_display_ili9488_renderer,
//                                                    ctx,
//                                                    base_x,
//                                                    y,
//                                                    6u,
//                                                    8u,
//                                                    base_x,
//                                                    y,
//                                                    1u,
//                                                    (unsigned char)text[i],
//                                                    fg);
//         if (st != GUI2C_STATUS_OK) return st;
//     }

//     return GUI2C_STATUS_OK;
// }

static gui2c_status_t ili9488_flush(void *ctx) {
    (void)ctx;
    return GUI2C_STATUS_OK;
}

const gui2c_renderer_vtable_t gui2c_display_ili9488_renderer = {
    .fill_rect = ili9488_fill_rect,
    .draw_pixel = ili9488_draw_pixel,
    .flush = ili9488_flush,
};

gui2c_status_t gui2c_display_ili9488_backlight(gui2c_display_ili9488_t *display, bool on) {
    if (!display || !display->cfg.gpio_write || display->cfg.pin_bl < 0) return GUI2C_STATUS_OK;
    display->cfg.gpio_write(display->cfg.user_ctx, display->cfg.pin_bl, on);
    return GUI2C_STATUS_OK;
}

gui2c_status_t gui2c_display_ili9488_init(gui2c_display_ili9488_t *display,
                                          const gui2c_display_ili9488_config_t *cfg) {
    static const uint8_t gamma_pos[] = {0x00,0x03,0x09,0x08,0x16,0x0A,0x3F,0x78,0x4C,0x09,0x0A,0x08,0x16,0x1A,0x0F};
    static const uint8_t gamma_neg[] = {0x00,0x16,0x19,0x03,0x0F,0x05,0x32,0x45,0x46,0x04,0x0E,0x0D,0x35,0x37,0x0F};
    uint8_t one;

    if (!display || !cfg || !cfg->spi_write || !cfg->gpio_write || !cfg->delay_ms) {
        return GUI2C_STATUS_ERR_INVALID_ARGUMENT;
    }

    memset(display, 0, sizeof(*display));
    display->cfg = *cfg;

    if (display->cfg.pin_cs >= 0) display->cfg.gpio_write(display->cfg.user_ctx, display->cfg.pin_cs, true);
    if (display->cfg.pin_dc >= 0) display->cfg.gpio_write(display->cfg.user_ctx, display->cfg.pin_dc, true);
    if (display->cfg.pin_rst >= 0) {
        display->cfg.gpio_write(display->cfg.user_ctx, display->cfg.pin_rst, true);
        ili9488_delay(display, 5);
        display->cfg.gpio_write(display->cfg.user_ctx, display->cfg.pin_rst, false);
        ili9488_delay(display, 20);
        display->cfg.gpio_write(display->cfg.user_ctx, display->cfg.pin_rst, true);
        ili9488_delay(display, 150);
    }

    ili9488_write_cmd(display, ILI9488_CMD_SWRESET);
    ili9488_delay(display, 150);
    ili9488_write_cmd(display, ILI9488_CMD_SLPOUT);
    ili9488_delay(display, 150);

    one = 0x66; ili9488_write_reg(display, ILI9488_CMD_COLMOD, &one, 1);
    one = ili9488_madctl_for_rotation(display); ili9488_write_reg(display, ILI9488_CMD_MADCTL, &one, 1);
    one = 0x01; ili9488_write_reg(display, ILI9488_CMD_GAMSET, &one, 1);
    ili9488_write_reg(display, ILI9488_CMD_PGAMCTRL, gamma_pos, sizeof(gamma_pos));
    ili9488_write_reg(display, ILI9488_CMD_NGAMCTRL, gamma_neg, sizeof(gamma_neg));
    if (display->cfg.invert_colors) ili9488_write_cmd(display, ILI9488_CMD_INVON);
    else ili9488_write_cmd(display, ILI9488_CMD_INVOFF);
    ili9488_write_cmd(display, ILI9488_CMD_DISPON);
    ili9488_delay(display, 50);
    gui2c_display_ili9488_backlight(display, true);

    return GUI2C_STATUS_OK;
}
