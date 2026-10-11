/*
 * SPDX-License-Identifier: MIT
 * Authors: any:frequency, seige.digital GbR, Leander Seige (2026)
 */

#include <math.h>
#include <string.h>

#include "lib/objects/guiwire_object_ring.h"
#include "guiwire_protocol_internal.h"
#include "lib/server/guiwire_server_objects.h"

typedef enum {
    GUIWIRE_RING_PIXEL_BG = 0,
    GUIWIRE_RING_PIXEL_INACTIVE = 1,
    GUIWIRE_RING_PIXEL_ACTIVE = 2
} guiwire_ring_pixel_class_t;

#define GUIWIRE_RING_BITMAP_MAX_DIAMETER 96u

typedef struct {
    bool valid;
    uint16_t radius;
    uint16_t angle_deg[GUIWIRE_RING_BITMAP_MAX_DIAMETER * GUIWIRE_RING_BITMAP_MAX_DIAMETER];
} guiwire_ring_angle_cache_t;

static guiwire_ring_angle_cache_t g_ring_angle_cache;

static guiwire_status_t parse_ring_desc(const uint8_t *payload, uint16_t payload_len, guiwire_ring_desc_t *out_desc) {
    if (!payload || !out_desc) return GUIWIRE_STATUS_ERR_INVALID_ARGUMENT;
    if (payload_len != 20u) return GUIWIRE_STATUS_ERR_PROTOCOL;

    out_desc->object_id = guiwire_read_u16le(&payload[0]);
    out_desc->cx = guiwire_read_u16le(&payload[2]);
    out_desc->cy = guiwire_read_u16le(&payload[4]);
    out_desc->radius = guiwire_read_u16le(&payload[6]);
    out_desc->inner_radius = guiwire_read_u16le(&payload[8]);
    out_desc->opening_angle_deg = guiwire_read_u16le(&payload[10]);
    out_desc->bg = guiwire_read_u16le(&payload[12]);
    out_desc->fg_active = guiwire_read_u16le(&payload[14]);
    out_desc->fg_inactive = guiwire_read_u16le(&payload[16]);
    out_desc->value = guiwire_read_u16le(&payload[18]);

    if (out_desc->inner_radius > out_desc->radius) return GUIWIRE_STATUS_ERR_PROTOCOL;
    if (out_desc->opening_angle_deg >= 360u) return GUIWIRE_STATUS_ERR_PROTOCOL;

    return GUIWIRE_STATUS_OK;
}

static guiwire_status_t parse_ring_update(const uint8_t *payload, uint16_t payload_len, guiwire_ring_update_t *out_update) {
    if (!payload || !out_update) return GUIWIRE_STATUS_ERR_INVALID_ARGUMENT;
    if (payload_len != 4u) return GUIWIRE_STATUS_ERR_PROTOCOL;

    out_update->object_id = guiwire_read_u16le(&payload[0]);
    out_update->value = guiwire_read_u16le(&payload[2]);
    return GUIWIRE_STATUS_OK;
}

static guiwire_status_t create_or_replace_ring(guiwire_server_t *server, const guiwire_ring_desc_t *desc, int replace_existing) {
    guiwire_ring_object_t *obj;

    if (!server || !desc) return GUIWIRE_STATUS_ERR_INVALID_ARGUMENT;

    obj = guiwire_server_find_ring_object(server, desc->object_id);
    if (obj && !replace_existing) return GUIWIRE_STATUS_ERR_ALREADY_EXISTS;
    if (!obj) obj = guiwire_server_alloc_ring_object(server, desc->object_id);
    if (!obj) return GUIWIRE_STATUS_ERR_NO_SPACE;

    obj->cx = desc->cx;
    obj->cy = desc->cy;
    obj->radius = desc->radius;
    obj->inner_radius = desc->inner_radius;
    obj->opening_angle_deg = desc->opening_angle_deg;
    obj->bg = desc->bg;
    obj->fg_active = desc->fg_active;
    obj->fg_inactive = desc->fg_inactive;
    obj->value = desc->value;

    return GUIWIRE_STATUS_OK;
}

static guiwire_status_t clear_ring_object(guiwire_server_t *server, const guiwire_ring_object_t *obj) {
    uint16_t size;
    uint16_t x;
    uint16_t y;

    if (!server || !obj) return GUIWIRE_STATUS_ERR_INVALID_ARGUMENT;

    size = (uint16_t)(obj->radius * 2u + 1u);
    x = (obj->cx > obj->radius) ? (uint16_t)(obj->cx - obj->radius) : 0u;
    y = (obj->cy > obj->radius) ? (uint16_t)(obj->cy - obj->radius) : 0u;
    return guiwire_server_fill_bg(server, x, y, size, size, obj->bg);
}

static double ring_progress_deg(int dx, int dy) {
    double angle = atan2((double)(-dx), (double)dy) * (180.0 / M_PI);
    if (angle < 0.0) angle += 360.0;
    return angle;
}

static void ring_prepare_angle_cache(uint16_t radius) {
    uint16_t size;

    if (g_ring_angle_cache.valid && g_ring_angle_cache.radius == radius) return;
    size = (uint16_t)(radius * 2u + 1u);
    if (size > GUIWIRE_RING_BITMAP_MAX_DIAMETER) return;

    for (uint16_t y = 0u; y < size; ++y) {
        for (uint16_t x = 0u; x < size; ++x) {
            int dx = (int)x - (int)radius;
            int dy = (int)y - (int)radius;
            double angle = ring_progress_deg(dx, dy);
            g_ring_angle_cache.angle_deg[(size_t)y * GUIWIRE_RING_BITMAP_MAX_DIAMETER + x] =
                (uint16_t)(angle + 0.5);
        }
    }
    g_ring_angle_cache.radius = radius;
    g_ring_angle_cache.valid = true;
}

static uint16_t ring_cached_angle(uint16_t radius, int dx, int dy) {
    uint16_t index_x = (uint16_t)(dx + (int)radius);
    uint16_t index_y = (uint16_t)(dy + (int)radius);
    return g_ring_angle_cache.angle_deg[(size_t)index_y * GUIWIRE_RING_BITMAP_MAX_DIAMETER + index_x];
}

static guiwire_ring_pixel_class_t ring_pixel_class(const guiwire_ring_object_t *obj, int dx, int dy) {
    uint32_t dist2;
    uint32_t outer2;
    uint32_t inner2;
    uint32_t visible_sweep;
    uint32_t filled_sweep;
    double progress;
    double start;
    double visible_pos;

    dist2 = (uint32_t)(dx * dx + dy * dy);
    outer2 = (uint32_t)obj->radius * (uint32_t)obj->radius;
    inner2 = (uint32_t)obj->inner_radius * (uint32_t)obj->inner_radius;

    if (dist2 > outer2 || dist2 < inner2) return GUIWIRE_RING_PIXEL_BG;

    visible_sweep = 360u - (uint32_t)obj->opening_angle_deg;
    if (visible_sweep == 0u) return GUIWIRE_RING_PIXEL_BG;

    progress = ring_progress_deg(dx, dy);
    start = ((double)obj->opening_angle_deg) * 0.5;
    visible_pos = progress - start;
    if (visible_pos < 0.0) visible_pos += 360.0;
    if (visible_pos >= (double)visible_sweep) return GUIWIRE_RING_PIXEL_BG;

    filled_sweep = ((uint32_t)obj->value * visible_sweep + 65534u) / 65535u;
    if (filled_sweep > visible_sweep) filled_sweep = visible_sweep;

    if (visible_pos < (double)filled_sweep || (obj->value == 0xFFFFu && visible_pos <= (double)visible_sweep)) {
        return GUIWIRE_RING_PIXEL_ACTIVE;
    }

    return GUIWIRE_RING_PIXEL_INACTIVE;
}

static guiwire_color_t ring_class_color(const guiwire_ring_object_t *obj, guiwire_ring_pixel_class_t pixel_class) {
    if (pixel_class == GUIWIRE_RING_PIXEL_ACTIVE) return obj->fg_active;
    if (pixel_class == GUIWIRE_RING_PIXEL_INACTIVE) return obj->fg_inactive;
    return obj->bg;
}

static guiwire_ring_pixel_class_t ring_bitmap_pixel_class(const guiwire_ring_object_t *obj,
                                                          int dx, int dy) {
    uint32_t dist2 = (uint32_t)(dx * dx + dy * dy);
    uint32_t outer2 = (uint32_t)obj->radius * (uint32_t)obj->radius;

    if (dist2 > outer2) return GUIWIRE_RING_PIXEL_BG;
    if (obj->inner_radius != 0u || obj->opening_angle_deg != 0u) {
        return ring_pixel_class(obj, dx, dy);
    }

    {
        uint32_t filled_sweep = ((uint32_t)obj->value * 360u + 65534u) / 65535u;
        uint16_t angle = ring_cached_angle(obj->radius, dx, dy);
        return ((uint32_t)angle < filled_sweep)
            ? GUIWIRE_RING_PIXEL_ACTIVE : GUIWIRE_RING_PIXEL_INACTIVE;
    }
}

static guiwire_status_t ring_draw_bitmap(guiwire_server_t *server,
                                         const guiwire_ring_object_t *obj) {
    uint16_t size;
    uint16_t x0;
    uint16_t y0;
    static guiwire_color_t pixels[GUIWIRE_RING_BITMAP_MAX_DIAMETER * GUIWIRE_RING_BITMAP_MAX_DIAMETER];

    if (!server || !obj || !server->cfg.renderer ||
        !server->cfg.renderer->blit_rgb565) return GUIWIRE_STATUS_ERR_NOT_SUPPORTED;
    size = (uint16_t)(obj->radius * 2u + 1u);
    if (size > GUIWIRE_RING_BITMAP_MAX_DIAMETER) return GUIWIRE_STATUS_ERR_NOT_SUPPORTED;
    x0 = (obj->cx > obj->radius) ? (uint16_t)(obj->cx - obj->radius) : 0u;
    y0 = (obj->cy > obj->radius) ? (uint16_t)(obj->cy - obj->radius) : 0u;
    if (obj->inner_radius == 0u && obj->opening_angle_deg == 0u) {
        ring_prepare_angle_cache(obj->radius);
    }

    for (uint16_t y = 0u; y < size; ++y) {
        for (uint16_t x = 0u; x < size; ++x) {
            int dx = (int)x - (int)obj->radius;
            int dy = (int)y - (int)obj->radius;
            pixels[(size_t)y * size + x] = ring_class_color(obj,
                ring_bitmap_pixel_class(obj, dx, dy));
        }
    }
    return server->cfg.renderer->blit_rgb565(server->cfg.renderer_ctx,
                                              x0, y0, size, size, pixels, size);
}

static guiwire_status_t ring_draw_column_diff(guiwire_server_t *server,
                                            const guiwire_ring_object_t *old_obj,
                                            const guiwire_ring_object_t *new_obj,
                                            int rel_x) {
    guiwire_status_t st = GUIWIRE_STATUS_OK;
    guiwire_ring_pixel_class_t current_new_class = GUIWIRE_RING_PIXEL_BG;
    int run_start = 0;
    uint16_t x0;
    uint16_t y0;
    uint16_t draw_x;
    int rel_y;

    if (!server || !new_obj) return GUIWIRE_STATUS_ERR_INVALID_ARGUMENT;

    x0 = (new_obj->cx > new_obj->radius) ? (uint16_t)(new_obj->cx - new_obj->radius) : 0u;
    y0 = (new_obj->cy > new_obj->radius) ? (uint16_t)(new_obj->cy - new_obj->radius) : 0u;
    draw_x = (uint16_t)((int)x0 + (rel_x + (int)new_obj->radius));

    for (rel_y = -(int)new_obj->radius; rel_y <= (int)new_obj->radius; ++rel_y) {
        guiwire_ring_pixel_class_t old_class =
            old_obj ? ring_pixel_class(old_obj, rel_x, rel_y) : GUIWIRE_RING_PIXEL_BG;
        guiwire_ring_pixel_class_t new_class = ring_pixel_class(new_obj, rel_x, rel_y);

        if (old_class == new_class) new_class = GUIWIRE_RING_PIXEL_BG;

        if (new_class != current_new_class) {
            if (current_new_class != GUIWIRE_RING_PIXEL_BG) {
                uint16_t draw_y = (uint16_t)((int)y0 + (run_start + (int)new_obj->radius));
                uint16_t draw_h = (uint16_t)(rel_y - run_start);
                st = guiwire_server_fill_bg(server,
                                          draw_x,
                                          draw_y,
                                          1u,
                                          draw_h,
                                          ring_class_color(new_obj, current_new_class));
                if (st != GUIWIRE_STATUS_OK) return st;
            }
            current_new_class = new_class;
            run_start = rel_y;
        }
    }

    if (current_new_class != GUIWIRE_RING_PIXEL_BG) {
        uint16_t draw_y = (uint16_t)((int)y0 + (run_start + (int)new_obj->radius));
        uint16_t draw_h = (uint16_t)((int)new_obj->radius - run_start + 1);
        st = guiwire_server_fill_bg(server,
                                  draw_x,
                                  draw_y,
                                  1u,
                                  draw_h,
                                  ring_class_color(new_obj, current_new_class));
        if (st != GUIWIRE_STATUS_OK) return st;
    }

    return GUIWIRE_STATUS_OK;
}

static guiwire_status_t guiwire_object_ring_draw_diff(guiwire_server_t *server,
                                                  const guiwire_ring_object_t *old_obj,
                                                  const guiwire_ring_object_t *new_obj) {
    int rel_x;

    if (!server || !new_obj || !new_obj->in_use) return GUIWIRE_STATUS_ERR_INVALID_ARGUMENT;
    if (!old_obj) return guiwire_object_ring_draw(server, new_obj);

    if (new_obj->radius == 0u) return GUIWIRE_STATUS_OK;

    if (server->cfg.renderer && server->cfg.renderer->blit_rgb565 &&
        new_obj->radius * 2u + 1u <= GUIWIRE_RING_BITMAP_MAX_DIAMETER) {
        return ring_draw_bitmap(server, new_obj);
    }

    for (rel_x = -(int)new_obj->radius; rel_x <= (int)new_obj->radius; ++rel_x) {
        guiwire_status_t st = ring_draw_column_diff(server, old_obj, new_obj, rel_x);
        if (st != GUIWIRE_STATUS_OK) return st;
    }

    return GUIWIRE_STATUS_OK;
}

guiwire_status_t guiwire_object_ring_draw(guiwire_server_t *server, const guiwire_ring_object_t *obj) {
    guiwire_status_t st;
    uint16_t size;
    uint16_t x0;
    uint16_t y0;
    int rel_x;

    if (!server || !obj || !obj->in_use) return GUIWIRE_STATUS_ERR_INVALID_ARGUMENT;

    if (server->cfg.renderer && server->cfg.renderer->blit_rgb565 &&
        obj->radius * 2u + 1u <= GUIWIRE_RING_BITMAP_MAX_DIAMETER) {
        return ring_draw_bitmap(server, obj);
    }

    st = clear_ring_object(server, obj);
    if (st != GUIWIRE_STATUS_OK) return st;
    if (obj->radius == 0u) return GUIWIRE_STATUS_OK;

    size = (uint16_t)(obj->radius * 2u + 1u);
    x0 = (obj->cx > obj->radius) ? (uint16_t)(obj->cx - obj->radius) : 0u;
    y0 = (obj->cy > obj->radius) ? (uint16_t)(obj->cy - obj->radius) : 0u;

    for (rel_x = -(int)obj->radius; rel_x <= (int)obj->radius; ++rel_x) {
        int rel_y;
        int run_start = 0;
        guiwire_ring_pixel_class_t current_class = GUIWIRE_RING_PIXEL_BG;
        uint16_t draw_x = (uint16_t)((int)x0 + (rel_x + (int)obj->radius));

        for (rel_y = -(int)obj->radius; rel_y <= (int)obj->radius; ++rel_y) {
            guiwire_ring_pixel_class_t pixel_class = ring_pixel_class(obj, rel_x, rel_y);

            if (pixel_class != current_class) {
                if (current_class != GUIWIRE_RING_PIXEL_BG) {
                    uint16_t draw_y = (uint16_t)((int)y0 + (run_start + (int)obj->radius));
                    uint16_t draw_h = (uint16_t)(rel_y - run_start);
                    st = guiwire_server_fill_bg(server, draw_x, draw_y, 1u, draw_h, ring_class_color(obj, current_class));
                    if (st != GUIWIRE_STATUS_OK) return st;
                }
                current_class = pixel_class;
                run_start = rel_y;
            }
        }

        if (current_class != GUIWIRE_RING_PIXEL_BG) {
            uint16_t draw_y = (uint16_t)((int)y0 + (run_start + (int)obj->radius));
            uint16_t draw_h = (uint16_t)((int)obj->radius - run_start + 1);
            st = guiwire_server_fill_bg(server, draw_x, draw_y, 1u, draw_h, ring_class_color(obj, current_class));
            if (st != GUIWIRE_STATUS_OK) return st;
        }
    }

    (void)size;
    return GUIWIRE_STATUS_OK;
}

guiwire_status_t guiwire_object_ring_create(guiwire_server_t *server, const uint8_t *payload, uint16_t payload_len) {
    guiwire_ring_desc_t desc;
    guiwire_ring_object_t *obj;
    guiwire_status_t st;

    st = parse_ring_desc(payload, payload_len, &desc);
    if (st != GUIWIRE_STATUS_OK) return st;

    obj = guiwire_server_find_ring_object(server, desc.object_id);
    if (obj) return GUIWIRE_STATUS_OK;

    st = create_or_replace_ring(server, &desc, 0);
    if (st != GUIWIRE_STATUS_OK) return st;

    obj = guiwire_server_find_ring_object(server, desc.object_id);
    st = guiwire_object_ring_draw(server, obj);
    if (st != GUIWIRE_STATUS_OK) return st;

    return guiwire_server_flush(server);
}

guiwire_status_t guiwire_object_ring_update(guiwire_server_t *server, const uint8_t *payload, uint16_t payload_len) {
    guiwire_ring_update_t update;
    guiwire_ring_object_t *obj;
    guiwire_ring_object_t old_obj;
    guiwire_status_t st;

    st = parse_ring_update(payload, payload_len, &update);
    if (st != GUIWIRE_STATUS_OK) return st;

    obj = guiwire_server_find_ring_object(server, update.object_id);
    if (!obj) return GUIWIRE_STATUS_ERR_NOT_FOUND;
    if (obj->value == update.value) return GUIWIRE_STATUS_OK;

    old_obj = *obj;
    obj->value = update.value;
    st = guiwire_object_ring_draw_diff(server, &old_obj, obj);
    if (st != GUIWIRE_STATUS_OK) return st;

    return guiwire_server_flush(server);
}

guiwire_status_t guiwire_object_ring_destroy(guiwire_server_t *server, guiwire_object_id_t object_id) {
    guiwire_ring_object_t *obj;
    guiwire_status_t st;

    obj = guiwire_server_find_ring_object(server, object_id);
    if (!obj) return GUIWIRE_STATUS_ERR_NOT_FOUND;

    st = clear_ring_object(server, obj);
    if (st != GUIWIRE_STATUS_OK) return st;

    memset(obj, 0, sizeof(*obj));
    return guiwire_server_flush(server);
}
