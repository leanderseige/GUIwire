
#include "lib/renderer/gui2c_renderer_text.h"
#include "lib/renderer/gui2c_font_5x7.h"

gui2c_status_t gui2c_renderer_draw_glyph_5x7_clipped(const gui2c_renderer_vtable_t *r,
                                                     void *ctx,
                                                     uint16_t clip_x,
                                                     uint16_t clip_y,
                                                     uint16_t clip_w,
                                                     uint16_t clip_h,
                                                     uint16_t base_x,
                                                     uint16_t base_y,
                                                     uint8_t zoom,
                                                     unsigned char ch,
                                                     gui2c_color_t fg) {
    const uint8_t *glyph;
    int col, row, zx, zy;
    if (!r || !r->draw_pixel) return GUI2C_STATUS_ERR_INVALID_ARGUMENT;
    if (zoom == 0u) zoom = 1u;
    glyph = gui2c_font_5x7_glyph(ch);

    for (col = 0; col < 5; ++col) {
        for (row = 0; row < 7; ++row) {
            if (glyph[col] & (1u << row)) {
                for (zx = 0; zx < zoom; ++zx) {
                    for (zy = 0; zy < zoom; ++zy) {
                        uint16_t px = (uint16_t)(base_x + col * zoom + zx);
                        uint16_t py = (uint16_t)(base_y + row * zoom + zy);
                        if (px < clip_x || px >= (uint16_t)(clip_x + clip_w)) continue;
                        if (py < clip_y || py >= (uint16_t)(clip_y + clip_h)) continue;
                        { gui2c_status_t st = r->draw_pixel(ctx, px, py, fg); if (st != GUI2C_STATUS_OK) return st; }
                    }
                }
            }
        }
    }
    return GUI2C_STATUS_OK;
}
