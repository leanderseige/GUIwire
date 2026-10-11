/*
 * SPDX-License-Identifier: MIT
 * Authors: any:frequency, seige.digital GbR, Leander Seige (2026)
 */


#ifndef GUIWIRE_RENDERER_TEXT_H
#define GUIWIRE_RENDERER_TEXT_H
#include "guiwire/guiwire_types.h"
#include "guiwire/guiwire_status.h"
#include "guiwire/guiwire_renderer.h"
#ifdef __cplusplus
extern "C" {
#endif
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
                                                     guiwire_color_t fg);
#ifdef __cplusplus
}
#endif
#endif
