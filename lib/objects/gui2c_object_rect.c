#include <string.h>
#include "lib/objects/gui2c_object_rect.h"
#include "lib/internal/gui2c_protocol_internal.h"
#include "lib/server/gui2c_server_objects.h"

static gui2c_status_t parse_rect_desc(const uint8_t *payload, uint16_t payload_len, gui2c_rect_desc_t *out_desc) {
    if (!payload || !out_desc) return GUI2C_STATUS_ERR_INVALID_ARGUMENT;
    if (payload_len != 15u) return GUI2C_STATUS_ERR_PROTOCOL;

    out_desc->object_id = gui2c_read_u16le(&payload[0]);
    out_desc->x = gui2c_read_u16le(&payload[2]);
    out_desc->y = gui2c_read_u16le(&payload[4]);
    out_desc->w = gui2c_read_u16le(&payload[6]);
    out_desc->h = gui2c_read_u16le(&payload[8]);
    out_desc->fill = gui2c_read_u16le(&payload[10]);
    out_desc->border = gui2c_read_u16le(&payload[12]);
    out_desc->flags = payload[14];

    return GUI2C_STATUS_OK;
}

static gui2c_status_t clear_rect_object(gui2c_server_t *server, const gui2c_rect_object_t *obj) {
    if (!server || !obj) return GUI2C_STATUS_ERR_INVALID_ARGUMENT;
    return gui2c_server_fill_bg(server, obj->x, obj->y, obj->w, obj->h, gui2c_rgb565(0,0,0));
}

static gui2c_status_t create_or_replace_rect(gui2c_server_t *server, const gui2c_rect_desc_t *desc, int replace_existing) {
    gui2c_rect_object_t *obj;

    if (!server || !desc) return GUI2C_STATUS_ERR_INVALID_ARGUMENT;

    obj = gui2c_server_find_rect_object(server, desc->object_id);
    if (obj && !replace_existing) return GUI2C_STATUS_ERR_ALREADY_EXISTS;
    if (!obj) obj = gui2c_server_alloc_rect_object(server, desc->object_id);
    if (!obj) return GUI2C_STATUS_ERR_NO_SPACE;

    obj->x = desc->x;
    obj->y = desc->y;
    obj->w = desc->w;
    obj->h = desc->h;
    obj->fill = desc->fill;
    obj->border = desc->border;
    obj->flags = desc->flags;

    return GUI2C_STATUS_OK;
}

gui2c_status_t gui2c_object_rect_draw(gui2c_server_t *server, const gui2c_rect_object_t *obj) {
    gui2c_status_t st;

    if (!server || !obj || !obj->in_use) return GUI2C_STATUS_ERR_INVALID_ARGUMENT;
    if (obj->w == 0u || obj->h == 0u) return GUI2C_STATUS_OK;

    if ((obj->flags & GUI2C_RECT_FLAG_FILL_TRANSPARENT) == 0u) {
        st = gui2c_server_fill_bg(server, obj->x, obj->y, obj->w, obj->h, obj->fill);
        if (st != GUI2C_STATUS_OK) return st;
    }

    if ((obj->flags & GUI2C_RECT_FLAG_BORDER_ENABLED) != 0u) {
        if (obj->w == 1u || obj->h == 1u) {
            st = gui2c_server_fill_bg(server, obj->x, obj->y, obj->w, obj->h, obj->border);
            if (st != GUI2C_STATUS_OK) return st;
        } else {
            st = gui2c_server_fill_bg(server, obj->x, obj->y, obj->w, 1u, obj->border);
            if (st != GUI2C_STATUS_OK) return st;

            st = gui2c_server_fill_bg(server, obj->x, (uint16_t)(obj->y + obj->h - 1u), obj->w, 1u, obj->border);
            if (st != GUI2C_STATUS_OK) return st;

            st = gui2c_server_fill_bg(server, obj->x, obj->y, 1u, obj->h, obj->border);
            if (st != GUI2C_STATUS_OK) return st;

            st = gui2c_server_fill_bg(server, (uint16_t)(obj->x + obj->w - 1u), obj->y, 1u, obj->h, obj->border);
            if (st != GUI2C_STATUS_OK) return st;
        }
    }

    return GUI2C_STATUS_OK;
}

gui2c_status_t gui2c_object_rect_create(gui2c_server_t *server, const uint8_t *payload, uint16_t payload_len) {
    gui2c_rect_desc_t desc;
    gui2c_rect_object_t *obj;
    gui2c_status_t st;

    st = parse_rect_desc(payload, payload_len, &desc);
    if (st != GUI2C_STATUS_OK) return st;

    obj = gui2c_server_find_rect_object(server, desc.object_id);
    if (obj) return GUI2C_STATUS_OK;

    st = create_or_replace_rect(server, &desc, 0);
    if (st != GUI2C_STATUS_OK) return st;

    obj = gui2c_server_find_rect_object(server, desc.object_id);
    st = gui2c_object_rect_draw(server, obj);
    if (st != GUI2C_STATUS_OK) return st;

    return gui2c_server_flush(server);
}

gui2c_status_t gui2c_object_rect_update(gui2c_server_t *server, const uint8_t *payload, uint16_t payload_len) {
    gui2c_rect_desc_t desc;
    gui2c_rect_object_t old_copy;
    gui2c_rect_object_t *obj;
    gui2c_status_t st;
    int had_old = 0;
    int need_clear_old = 0;

    st = parse_rect_desc(payload, payload_len, &desc);
    if (st != GUI2C_STATUS_OK) return st;

    obj = gui2c_server_find_rect_object(server, desc.object_id);
    if (obj) {
        old_copy = *obj;
        had_old = 1;
    }

    st = create_or_replace_rect(server, &desc, 1);
    if (st != GUI2C_STATUS_OK) return st;

    obj = gui2c_server_find_rect_object(server, desc.object_id);
    if (!obj) return GUI2C_STATUS_ERR_NOT_FOUND;

    if (had_old) {
        if ((desc.flags & GUI2C_RECT_FLAG_FILL_TRANSPARENT) != 0u) {
            need_clear_old = 1;
        } else if (!rect_geometry_equal(&old_copy, &desc)) {
            need_clear_old = 1;
        }
    }

    if (need_clear_old) {
        st = clear_rect_object(server, &old_copy);
        if (st != GUI2C_STATUS_OK) return st;
    }

    st = gui2c_object_rect_draw(server, obj);
    if (st != GUI2C_STATUS_OK) return st;

    return gui2c_server_flush(server);
}

gui2c_status_t gui2c_object_rect_destroy(gui2c_server_t *server, gui2c_object_id_t object_id) {
    gui2c_rect_object_t *obj;
    gui2c_status_t st;

    obj = gui2c_server_find_rect_object(server, object_id);
    if (!obj) return GUI2C_STATUS_ERR_NOT_FOUND;

    st = clear_rect_object(server, obj);
    if (st != GUI2C_STATUS_OK) return st;

    memset(obj, 0, sizeof(*obj));

    return gui2c_server_flush(server);
}

static int rect_geometry_equal(const gui2c_rect_object_t *a, const gui2c_rect_desc_t *b) {
    if (!a || !b) return 0;
    return a->x == b->x &&
           a->y == b->y &&
           a->w == b->w &&
           a->h == b->h;
}
