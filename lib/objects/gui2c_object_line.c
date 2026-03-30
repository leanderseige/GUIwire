#include <stdlib.h>
#include <string.h>

#include "lib/objects/gui2c_object_line.h"
#include "lib/internal/gui2c_protocol_internal.h"
#include "lib/server/gui2c_server_objects.h"

static gui2c_status_t parse_line_desc(const uint8_t *payload, uint16_t payload_len, gui2c_line_desc_t *out_desc) {
    if (!payload || !out_desc) return GUI2C_STATUS_ERR_INVALID_ARGUMENT;
    if (payload_len != 15u) return GUI2C_STATUS_ERR_PROTOCOL;

    out_desc->object_id = gui2c_read_u16le(&payload[0]);
    out_desc->x1 = gui2c_read_u16le(&payload[2]);
    out_desc->y1 = gui2c_read_u16le(&payload[4]);
    out_desc->x2 = gui2c_read_u16le(&payload[6]);
    out_desc->y2 = gui2c_read_u16le(&payload[8]);
    out_desc->fg = gui2c_read_u16le(&payload[10]);
    out_desc->bg = gui2c_read_u16le(&payload[12]);
    out_desc->thickness = payload[14];

    return GUI2C_STATUS_OK;
}

static gui2c_status_t create_or_replace_line(gui2c_server_t *server, const gui2c_line_desc_t *desc, int replace_existing) {
    gui2c_line_object_t *obj;

    if (!server || !desc) return GUI2C_STATUS_ERR_INVALID_ARGUMENT;

    obj = gui2c_server_find_line_object(server, desc->object_id);
    if (obj && !replace_existing) return GUI2C_STATUS_ERR_ALREADY_EXISTS;
    if (!obj) obj = gui2c_server_alloc_line_object(server, desc->object_id);
    if (!obj) return GUI2C_STATUS_ERR_NO_SPACE;

    obj->x1 = desc->x1;
    obj->y1 = desc->y1;
    obj->x2 = desc->x2;
    obj->y2 = desc->y2;
    obj->fg = desc->fg;
    obj->bg = desc->bg;
    obj->thickness = desc->thickness ? desc->thickness : 1u;

    return GUI2C_STATUS_OK;
}

static gui2c_status_t draw_thick_point(gui2c_server_t *server, int x, int y, uint8_t thickness, gui2c_color_t color) {
    uint16_t width = thickness ? thickness : 1u;
    uint16_t height = width;
    int half = (int)width / 2;
    int draw_x = x - half;
    int draw_y = y - half;

    if (!server) return GUI2C_STATUS_ERR_INVALID_ARGUMENT;

    if (width == 1u) {
        return server->cfg.renderer->draw_pixel(server->cfg.renderer_ctx, x, y, color);
    }

    if (draw_x < 0) {
        width = (uint16_t)(width + draw_x);
        draw_x = 0;
    }
    if (draw_y < 0) {
        height = (uint16_t)(height + draw_y);
        draw_y = 0;
    }
    if ((int)width <= 0 || (int)height <= 0) return GUI2C_STATUS_OK;

    return gui2c_server_fill_bg(server, (uint16_t)draw_x, (uint16_t)draw_y, width, height, color);
}

static gui2c_status_t draw_vertical_span(gui2c_server_t *server,
                                         int x,
                                         int y0,
                                         int y1,
                                         uint8_t thickness,
                                         gui2c_color_t color) {
    uint16_t width = thickness ? thickness : 1u;
    int half = (int)width / 2;
    int draw_x = x - half;
    int top = (y0 < y1) ? y0 : y1;
    int bottom = (y0 > y1) ? y0 : y1;
    uint16_t draw_w = width;
    uint16_t draw_h;

    if (!server) return GUI2C_STATUS_ERR_INVALID_ARGUMENT;

    if (draw_x < 0) {
        draw_w = (uint16_t)(draw_w + draw_x);
        draw_x = 0;
    }
    if (top < 0) top = 0;
    if ((int)draw_w <= 0 || bottom < top) return GUI2C_STATUS_OK;

    draw_h = (uint16_t)(bottom - top + 1);
    return gui2c_server_fill_bg(server, (uint16_t)draw_x, (uint16_t)top, draw_w, draw_h, color);
}

static gui2c_status_t draw_horizontal_span(gui2c_server_t *server,
                                           int x0,
                                           int x1,
                                           int y,
                                           uint8_t thickness,
                                           gui2c_color_t color) {
    uint16_t height = thickness ? thickness : 1u;
    int half = (int)height / 2;
    int draw_y = y - half;
    int left = (x0 < x1) ? x0 : x1;
    int right = (x0 > x1) ? x0 : x1;
    uint16_t draw_h = height;
    uint16_t draw_w;

    if (!server) return GUI2C_STATUS_ERR_INVALID_ARGUMENT;

    if (draw_y < 0) {
        draw_h = (uint16_t)(draw_h + draw_y);
        draw_y = 0;
    }
    if (left < 0) left = 0;
    if ((int)draw_h <= 0 || right < left) return GUI2C_STATUS_OK;

    draw_w = (uint16_t)(right - left + 1);
    return gui2c_server_fill_bg(server, (uint16_t)left, (uint16_t)draw_y, draw_w, draw_h, color);
}

static gui2c_status_t draw_line_spans(gui2c_server_t *server,
                                      int x0,
                                      int y0,
                                      int x1,
                                      int y1,
                                      uint8_t thickness,
                                      gui2c_color_t color) {
    int left_x;
    int left_y;
    int right_x;
    int right_y;
    int dx;
    int x;

    if (!server) return GUI2C_STATUS_ERR_INVALID_ARGUMENT;

    if (x0 == x1) {
        return draw_vertical_span(server, x0, y0, y1, thickness, color);
    }
    if (y0 == y1) {
        return draw_horizontal_span(server, x0, x1, y0, thickness, color);
    }

    if (x0 < x1) {
        left_x = x0;
        left_y = y0;
        right_x = x1;
        right_y = y1;
    } else {
        left_x = x1;
        left_y = y1;
        right_x = x0;
        right_y = y0;
    }

    dx = right_x - left_x;
    for (x = left_x; x <= right_x; ++x) {
        int rel0 = x - left_x;
        int rel1 = (x < right_x) ? (rel0 + 1) : rel0;
        int ya = left_y + ((right_y - left_y) * rel0) / dx;
        int yb = left_y + ((right_y - left_y) * rel1) / dx;
        gui2c_status_t st = draw_vertical_span(server, x, ya, yb, thickness, color);
        if (st != GUI2C_STATUS_OK) return st;
    }

    return GUI2C_STATUS_OK;
}

gui2c_status_t gui2c_object_line_draw(gui2c_server_t *server, const gui2c_line_object_t *obj, gui2c_color_t color) {
    if (!server || !obj || !obj->in_use) {
        return GUI2C_STATUS_ERR_INVALID_ARGUMENT;
    }
    return draw_line_spans(server, (int)obj->x1, (int)obj->y1, (int)obj->x2, (int)obj->y2, obj->thickness, color);
}

gui2c_status_t gui2c_object_line_create(gui2c_server_t *server, const uint8_t *payload, uint16_t payload_len) {
    gui2c_line_desc_t desc;
    gui2c_line_object_t *obj;
    gui2c_status_t st;

    st = parse_line_desc(payload, payload_len, &desc);
    if (st != GUI2C_STATUS_OK) return st;

    obj = gui2c_server_find_line_object(server, desc.object_id);
    if (obj) return GUI2C_STATUS_OK;

    st = create_or_replace_line(server, &desc, 0);
    if (st != GUI2C_STATUS_OK) return st;

    obj = gui2c_server_find_line_object(server, desc.object_id);
    st = gui2c_object_line_draw(server, obj, obj->fg);
    if (st != GUI2C_STATUS_OK) return st;

    return gui2c_server_flush(server);
}

gui2c_status_t gui2c_object_line_update(gui2c_server_t *server, const uint8_t *payload, uint16_t payload_len) {
    gui2c_line_desc_t desc;
    gui2c_line_object_t old_copy;
    gui2c_line_object_t *obj;
    gui2c_status_t st;
    int had_old = 0;

    st = parse_line_desc(payload, payload_len, &desc);
    if (st != GUI2C_STATUS_OK) return st;

    obj = gui2c_server_find_line_object(server, desc.object_id);
    if (obj) {
        old_copy = *obj;
        had_old = 1;
    }

    st = create_or_replace_line(server, &desc, 1);
    if (st != GUI2C_STATUS_OK) return st;

    obj = gui2c_server_find_line_object(server, desc.object_id);
    if (!obj) return GUI2C_STATUS_ERR_NOT_FOUND;

    if (had_old) {
        st = gui2c_object_line_draw(server, &old_copy, old_copy.bg);
        if (st != GUI2C_STATUS_OK) return st;
    }

    st = gui2c_object_line_draw(server, obj, obj->fg);
    if (st != GUI2C_STATUS_OK) return st;

    return gui2c_server_flush(server);
}

gui2c_status_t gui2c_object_line_destroy(gui2c_server_t *server, gui2c_object_id_t object_id) {
    gui2c_line_object_t *obj;
    gui2c_status_t st;

    obj = gui2c_server_find_line_object(server, object_id);
    if (!obj) return GUI2C_STATUS_ERR_NOT_FOUND;

    st = gui2c_object_line_draw(server, obj, obj->bg);
    if (st != GUI2C_STATUS_OK) return st;

    memset(obj, 0, sizeof(*obj));

    return gui2c_server_flush(server);
}
