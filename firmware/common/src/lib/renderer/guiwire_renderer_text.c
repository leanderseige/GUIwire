/*
 * SPDX-License-Identifier: MIT
 * Authors: any:frequency, seige.digital GbR, Leander Seige (2026)
 */


#include "lib/renderer/guiwire_renderer_text.h"
#include "lib/renderer/guiwire_font_5x7.h"

guiwire_status_t guiwire_renderer_draw_glyph_5x7_clipped(const guiwire_renderer_vtable_t *r,
                                                     void *ctx,
                                                     uint16_t clip_x,
                                                     uint16_t clip_y,
                                                     uint16_t clip_w,
                                                     uint16_t clip_h,
                                                     uint16_t base_x,
                                                     uint16_t base_y,
                                                     uint8_t zoom,
                                                     unsigned char ch,
                                                     guiwire_color_t fg) {
    const uint8_t *glyph;
    int row;
    if (!r || (!r->fill_rect && !r->draw_pixel)) return GUIWIRE_STATUS_ERR_INVALID_ARGUMENT;
    if (zoom == 0u) zoom = 1u;
    glyph = guiwire_font_5x7_glyph(ch);

    for (row = 0; row < 7; ++row) {
        int col = 0;

        while (col < 5) {
            if ((glyph[col] & (1u << row)) == 0u) {
                ++col;
                continue;
            }

            {
                int run_start = col;
                int run_end = col + 1;
                uint16_t px;
                uint16_t py;
                uint16_t pw;
                uint16_t ph;

                while (run_end < 5 && (glyph[run_end] & (1u << row)) != 0u) {
                    ++run_end;
                }

                px = (uint16_t)(base_x + run_start * zoom);
                py = (uint16_t)(base_y + row * zoom);
                pw = (uint16_t)((run_end - run_start) * zoom);
                ph = zoom;

                if (px < clip_x) {
                    uint16_t delta = (uint16_t)(clip_x - px);
                    if (delta >= pw) {
                        col = run_end;
                        continue;
                    }
                    px = clip_x;
                    pw = (uint16_t)(pw - delta);
                }
                if (py < clip_y) {
                    uint16_t delta = (uint16_t)(clip_y - py);
                    if (delta >= ph) {
                        col = run_end;
                        continue;
                    }
                    py = clip_y;
                    ph = (uint16_t)(ph - delta);
                }
                if (px >= (uint16_t)(clip_x + clip_w) || py >= (uint16_t)(clip_y + clip_h)) {
                    col = run_end;
                    continue;
                }
                if ((uint32_t)px + pw > (uint32_t)(clip_x + clip_w)) {
                    pw = (uint16_t)((clip_x + clip_w) - px);
                }
                if ((uint32_t)py + ph > (uint32_t)(clip_y + clip_h)) {
                    ph = (uint16_t)((clip_y + clip_h) - py);
                }
                if (pw == 0u || ph == 0u) {
                    col = run_end;
                    continue;
                }

                if (r->fill_rect) {
                    guiwire_status_t st = r->fill_rect(ctx, px, py, pw, ph, fg);
                    if (st != GUIWIRE_STATUS_OK) return st;
                } else {
                    int zx;
                    int zy;
                    for (zx = 0; zx < pw; ++zx) {
                        for (zy = 0; zy < ph; ++zy) {
                            guiwire_status_t st = r->draw_pixel(ctx, (int)(px + zx), (int)(py + zy), fg);
                            if (st != GUIWIRE_STATUS_OK) return st;
                        }
                    }
                }

                col = run_end;
            }
        }
    }
    return GUIWIRE_STATUS_OK;
}
