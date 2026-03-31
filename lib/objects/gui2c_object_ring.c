#include <math.h>
#include <string.h>

#include "lib/objects/gui2c_object_ring.h"
#include "lib/internal/gui2c_protocol_internal.h"
#include "lib/server/gui2c_server_objects.h"

typedef enum {
    GUI2C_RING_PIXEL_BG = 0,
    GUI2C_RING_PIXEL_INACTIVE = 1,
    GUI2C_RING_PIXEL_ACTIVE = 2
} gui2c_ring_pixel_class_t;

static gui2c_status_t parse_ring_desc(const uint8_t *payload, uint16_t payload_len, gui2c_ring_desc_t *out_desc) {
    if (!payload || !out_desc) return GUI2C_STATUS_ERR_INVALID_ARGUMENT;
    if (payload_len != 20u) return GUI2C_STATUS_ERR_PROTOCOL;

    out_desc->object_id = gui2c_read_u16le(&payload[0]);
    out_desc->cx = gui2c_read_u16le(&payload[2]);
    out_desc->cy = gui2c_read_u16le(&payload[4]);
    out_desc->radius = gui2c_read_u16le(&payload[6]);
    out_desc->inner_radius = gui2c_read_u16le(&payload[8]);
    out_desc->opening_angle_deg = gui2c_read_u16le(&payload[10]);
    out_desc->bg = gui2c_read_u16le(&payload[12]);
    out_desc->fg_active = gui2c_read_u16le(&payload[14]);
    out_desc->fg_inactive = gui2c_read_u16le(&payload[16]);
    out_desc->value = gui2c_read_u16le(&payload[18]);

    if (out_desc->inner_radius > out_desc->radius) return GUI2C_STATUS_ERR_PROTOCOL;
    if (out_desc->opening_angle_deg >= 360u) return GUI2C_STATUS_ERR_PROTOCOL;

    return GUI2C_STATUS_OK;
}

static gui2c_status_t parse_ring_update(const uint8_t *payload, uint16_t payload_len, gui2c_ring_update_t *out_update) {
    if (!payload || !out_update) return GUI2C_STATUS_ERR_INVALID_ARGUMENT;
    if (payload_len != 4u) return GUI2C_STATUS_ERR_PROTOCOL;

    out_update->object_id = gui2c_read_u16le(&payload[0]);
    out_update->value = gui2c_read_u16le(&payload[2]);
    return GUI2C_STATUS_OK;
}

static gui2c_status_t create_or_replace_ring(gui2c_server_t *server, const gui2c_ring_desc_t *desc, int replace_existing) {
    gui2c_ring_object_t *obj;

    if (!server || !desc) return GUI2C_STATUS_ERR_INVALID_ARGUMENT;

    obj = gui2c_server_find_ring_object(server, desc->object_id);
    if (obj && !replace_existing) return GUI2C_STATUS_ERR_ALREADY_EXISTS;
    if (!obj) obj = gui2c_server_alloc_ring_object(server, desc->object_id);
    if (!obj) return GUI2C_STATUS_ERR_NO_SPACE;

    obj->cx = desc->cx;
    obj->cy = desc->cy;
    obj->radius = desc->radius;
    obj->inner_radius = desc->inner_radius;
    obj->opening_angle_deg = desc->opening_angle_deg;
    obj->bg = desc->bg;
    obj->fg_active = desc->fg_active;
    obj->fg_inactive = desc->fg_inactive;
    obj->value = desc->value;

    return GUI2C_STATUS_OK;
}

static gui2c_status_t clear_ring_object(gui2c_server_t *server, const gui2c_ring_object_t *obj) {
    uint16_t size;
    uint16_t x;
    uint16_t y;

    if (!server || !obj) return GUI2C_STATUS_ERR_INVALID_ARGUMENT;

    size = (uint16_t)(obj->radius * 2u + 1u);
    x = (obj->cx > obj->radius) ? (uint16_t)(obj->cx - obj->radius) : 0u;
    y = (obj->cy > obj->radius) ? (uint16_t)(obj->cy - obj->radius) : 0u;
    return gui2c_server_fill_bg(server, x, y, size, size, obj->bg);
}

static double ring_progress_deg(int dx, int dy) {
    double angle = atan2((double)(-dx), (double)dy) * (180.0 / M_PI);
    if (angle < 0.0) angle += 360.0;
    return angle;
}

static gui2c_ring_pixel_class_t ring_pixel_class(const gui2c_ring_object_t *obj, int dx, int dy) {
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

    if (dist2 > outer2 || dist2 < inner2) return GUI2C_RING_PIXEL_BG;

    visible_sweep = 360u - (uint32_t)obj->opening_angle_deg;
    if (visible_sweep == 0u) return GUI2C_RING_PIXEL_BG;

    progress = ring_progress_deg(dx, dy);
    start = ((double)obj->opening_angle_deg) * 0.5;
    visible_pos = progress - start;
    if (visible_pos < 0.0) visible_pos += 360.0;
    if (visible_pos >= (double)visible_sweep) return GUI2C_RING_PIXEL_BG;

    filled_sweep = ((uint32_t)obj->value * visible_sweep + 65534u) / 65535u;
    if (filled_sweep > visible_sweep) filled_sweep = visible_sweep;

    if (visible_pos < (double)filled_sweep || (obj->value == 0xFFFFu && visible_pos <= (double)visible_sweep)) {
        return GUI2C_RING_PIXEL_ACTIVE;
    }

    return GUI2C_RING_PIXEL_INACTIVE;
}

static gui2c_color_t ring_class_color(const gui2c_ring_object_t *obj, gui2c_ring_pixel_class_t pixel_class) {
    if (pixel_class == GUI2C_RING_PIXEL_ACTIVE) return obj->fg_active;
    if (pixel_class == GUI2C_RING_PIXEL_INACTIVE) return obj->fg_inactive;
    return obj->bg;
}

static gui2c_status_t ring_draw_column_diff(gui2c_server_t *server,
                                            const gui2c_ring_object_t *old_obj,
                                            const gui2c_ring_object_t *new_obj,
                                            int rel_x) {
    gui2c_status_t st = GUI2C_STATUS_OK;
    gui2c_ring_pixel_class_t current_new_class = GUI2C_RING_PIXEL_BG;
    int run_start = 0;
    uint16_t x0;
    uint16_t y0;
    uint16_t draw_x;
    int rel_y;

    if (!server || !new_obj) return GUI2C_STATUS_ERR_INVALID_ARGUMENT;

    x0 = (new_obj->cx > new_obj->radius) ? (uint16_t)(new_obj->cx - new_obj->radius) : 0u;
    y0 = (new_obj->cy > new_obj->radius) ? (uint16_t)(new_obj->cy - new_obj->radius) : 0u;
    draw_x = (uint16_t)((int)x0 + (rel_x + (int)new_obj->radius));

    for (rel_y = -(int)new_obj->radius; rel_y <= (int)new_obj->radius; ++rel_y) {
        gui2c_ring_pixel_class_t old_class =
            old_obj ? ring_pixel_class(old_obj, rel_x, rel_y) : GUI2C_RING_PIXEL_BG;
        gui2c_ring_pixel_class_t new_class = ring_pixel_class(new_obj, rel_x, rel_y);

        if (old_class == new_class) new_class = GUI2C_RING_PIXEL_BG;

        if (new_class != current_new_class) {
            if (current_new_class != GUI2C_RING_PIXEL_BG) {
                uint16_t draw_y = (uint16_t)((int)y0 + (run_start + (int)new_obj->radius));
                uint16_t draw_h = (uint16_t)(rel_y - run_start);
                st = gui2c_server_fill_bg(server,
                                          draw_x,
                                          draw_y,
                                          1u,
                                          draw_h,
                                          ring_class_color(new_obj, current_new_class));
                if (st != GUI2C_STATUS_OK) return st;
            }
            current_new_class = new_class;
            run_start = rel_y;
        }
    }

    if (current_new_class != GUI2C_RING_PIXEL_BG) {
        uint16_t draw_y = (uint16_t)((int)y0 + (run_start + (int)new_obj->radius));
        uint16_t draw_h = (uint16_t)((int)new_obj->radius - run_start + 1);
        st = gui2c_server_fill_bg(server,
                                  draw_x,
                                  draw_y,
                                  1u,
                                  draw_h,
                                  ring_class_color(new_obj, current_new_class));
        if (st != GUI2C_STATUS_OK) return st;
    }

    return GUI2C_STATUS_OK;
}

static gui2c_status_t gui2c_object_ring_draw_diff(gui2c_server_t *server,
                                                  const gui2c_ring_object_t *old_obj,
                                                  const gui2c_ring_object_t *new_obj) {
    int rel_x;

    if (!server || !new_obj || !new_obj->in_use) return GUI2C_STATUS_ERR_INVALID_ARGUMENT;
    if (!old_obj) return gui2c_object_ring_draw(server, new_obj);

    if (new_obj->radius == 0u) return GUI2C_STATUS_OK;

    for (rel_x = -(int)new_obj->radius; rel_x <= (int)new_obj->radius; ++rel_x) {
        gui2c_status_t st = ring_draw_column_diff(server, old_obj, new_obj, rel_x);
        if (st != GUI2C_STATUS_OK) return st;
    }

    return GUI2C_STATUS_OK;
}

gui2c_status_t gui2c_object_ring_draw(gui2c_server_t *server, const gui2c_ring_object_t *obj) {
    gui2c_status_t st;
    uint16_t size;
    uint16_t x0;
    uint16_t y0;
    int rel_x;

    if (!server || !obj || !obj->in_use) return GUI2C_STATUS_ERR_INVALID_ARGUMENT;

    st = clear_ring_object(server, obj);
    if (st != GUI2C_STATUS_OK) return st;
    if (obj->radius == 0u) return GUI2C_STATUS_OK;

    size = (uint16_t)(obj->radius * 2u + 1u);
    x0 = (obj->cx > obj->radius) ? (uint16_t)(obj->cx - obj->radius) : 0u;
    y0 = (obj->cy > obj->radius) ? (uint16_t)(obj->cy - obj->radius) : 0u;

    for (rel_x = -(int)obj->radius; rel_x <= (int)obj->radius; ++rel_x) {
        int rel_y;
        int run_start = 0;
        gui2c_ring_pixel_class_t current_class = GUI2C_RING_PIXEL_BG;
        uint16_t draw_x = (uint16_t)((int)x0 + (rel_x + (int)obj->radius));

        for (rel_y = -(int)obj->radius; rel_y <= (int)obj->radius; ++rel_y) {
            gui2c_ring_pixel_class_t pixel_class = ring_pixel_class(obj, rel_x, rel_y);

            if (pixel_class != current_class) {
                if (current_class != GUI2C_RING_PIXEL_BG) {
                    uint16_t draw_y = (uint16_t)((int)y0 + (run_start + (int)obj->radius));
                    uint16_t draw_h = (uint16_t)(rel_y - run_start);
                    st = gui2c_server_fill_bg(server, draw_x, draw_y, 1u, draw_h, ring_class_color(obj, current_class));
                    if (st != GUI2C_STATUS_OK) return st;
                }
                current_class = pixel_class;
                run_start = rel_y;
            }
        }

        if (current_class != GUI2C_RING_PIXEL_BG) {
            uint16_t draw_y = (uint16_t)((int)y0 + (run_start + (int)obj->radius));
            uint16_t draw_h = (uint16_t)((int)obj->radius - run_start + 1);
            st = gui2c_server_fill_bg(server, draw_x, draw_y, 1u, draw_h, ring_class_color(obj, current_class));
            if (st != GUI2C_STATUS_OK) return st;
        }
    }

    (void)size;
    return GUI2C_STATUS_OK;
}

gui2c_status_t gui2c_object_ring_create(gui2c_server_t *server, const uint8_t *payload, uint16_t payload_len) {
    gui2c_ring_desc_t desc;
    gui2c_ring_object_t *obj;
    gui2c_status_t st;

    st = parse_ring_desc(payload, payload_len, &desc);
    if (st != GUI2C_STATUS_OK) return st;

    obj = gui2c_server_find_ring_object(server, desc.object_id);
    if (obj) return GUI2C_STATUS_OK;

    st = create_or_replace_ring(server, &desc, 0);
    if (st != GUI2C_STATUS_OK) return st;

    obj = gui2c_server_find_ring_object(server, desc.object_id);
    st = gui2c_object_ring_draw(server, obj);
    if (st != GUI2C_STATUS_OK) return st;

    return gui2c_server_flush(server);
}

gui2c_status_t gui2c_object_ring_update(gui2c_server_t *server, const uint8_t *payload, uint16_t payload_len) {
    gui2c_ring_update_t update;
    gui2c_ring_object_t *obj;
    gui2c_ring_object_t old_obj;
    gui2c_status_t st;

    st = parse_ring_update(payload, payload_len, &update);
    if (st != GUI2C_STATUS_OK) return st;

    obj = gui2c_server_find_ring_object(server, update.object_id);
    if (!obj) return GUI2C_STATUS_ERR_NOT_FOUND;
    if (obj->value == update.value) return GUI2C_STATUS_OK;

    old_obj = *obj;
    obj->value = update.value;
    st = gui2c_object_ring_draw_diff(server, &old_obj, obj);
    if (st != GUI2C_STATUS_OK) return st;

    return gui2c_server_flush(server);
}

gui2c_status_t gui2c_object_ring_destroy(gui2c_server_t *server, gui2c_object_id_t object_id) {
    gui2c_ring_object_t *obj;
    gui2c_status_t st;

    obj = gui2c_server_find_ring_object(server, object_id);
    if (!obj) return GUI2C_STATUS_ERR_NOT_FOUND;

    st = clear_ring_object(server, obj);
    if (st != GUI2C_STATUS_OK) return st;

    memset(obj, 0, sizeof(*obj));
    return gui2c_server_flush(server);
}
