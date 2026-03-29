
#ifndef GUI2C_RENDERER_TEXT_H
#define GUI2C_RENDERER_TEXT_H
#include "gui2c/gui2c.h"
#ifdef __cplusplus
extern "C" {
#endif
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
                                                     gui2c_color_t fg);
#ifdef __cplusplus
}
#endif
#endif
