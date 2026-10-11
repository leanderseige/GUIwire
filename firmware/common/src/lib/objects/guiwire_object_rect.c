/*
 * SPDX-License-Identifier: MIT
 * Authors: any:frequency, seige.digital GbR, Leander Seige (2026)
 */

#include <string.h>
#include "lib/objects/guiwire_object_rect.h"
#include "guiwire_protocol_internal.h"
#include "lib/server/guiwire_server_objects.h"

static guiwire_status_t parse_rect_desc(const uint8_t *payload, uint16_t payload_len, guiwire_rect_desc_t *out_desc) {
    if (!payload || !out_desc) return GUIWIRE_STATUS_ERR_INVALID_ARGUMENT;
    if (payload_len != 15u) return GUIWIRE_STATUS_ERR_PROTOCOL;

    out_desc->object_id = guiwire_read_u16le(&payload[0]);
    out_desc->x = guiwire_read_u16le(&payload[2]);
    out_desc->y = guiwire_read_u16le(&payload[4]);
    out_desc->w = guiwire_read_u16le(&payload[6]);
    out_desc->h = guiwire_read_u16le(&payload[8]);
    out_desc->fill = guiwire_read_u16le(&payload[10]);
    out_desc->border = guiwire_read_u16le(&payload[12]);
    out_desc->flags = payload[14];

    return GUIWIRE_STATUS_OK;
}

static guiwire_status_t parse_rect_update(const uint8_t *payload, uint16_t payload_len, guiwire_rect_update_t *out_update) {
    if (!payload || !out_update) return GUIWIRE_STATUS_ERR_INVALID_ARGUMENT;
    if (payload_len != 7u) return GUIWIRE_STATUS_ERR_PROTOCOL;

    out_update->object_id = guiwire_read_u16le(&payload[0]);
    out_update->fill = guiwire_read_u16le(&payload[2]);
    out_update->border = guiwire_read_u16le(&payload[4]);
    out_update->flags = payload[6];

    return GUIWIRE_STATUS_OK;
}

static guiwire_status_t clear_rect_object(guiwire_server_t *server, const guiwire_rect_object_t *obj) {
    if (!server || !obj) return GUIWIRE_STATUS_ERR_INVALID_ARGUMENT;
    return guiwire_server_fill_bg(server, obj->x, obj->y, obj->w, obj->h, guiwire_rgb565(0,0,0));
}

static guiwire_status_t create_or_replace_rect(guiwire_server_t *server, const guiwire_rect_desc_t *desc, int replace_existing) {
    guiwire_rect_object_t *obj;

    if (!server || !desc) return GUIWIRE_STATUS_ERR_INVALID_ARGUMENT;

    obj = guiwire_server_find_rect_object(server, desc->object_id);
    if (obj && !replace_existing) return GUIWIRE_STATUS_ERR_ALREADY_EXISTS;
    if (!obj) obj = guiwire_server_alloc_rect_object(server, desc->object_id);
    if (!obj) return GUIWIRE_STATUS_ERR_NO_SPACE;

    obj->x = desc->x;
    obj->y = desc->y;
    obj->w = desc->w;
    obj->h = desc->h;
    obj->fill = desc->fill;
    obj->border = desc->border;
    obj->flags = desc->flags;

    return GUIWIRE_STATUS_OK;
}

guiwire_status_t guiwire_object_rect_draw(guiwire_server_t *server, const guiwire_rect_object_t *obj) {
    guiwire_status_t st;

    if (!server || !obj || !obj->in_use) return GUIWIRE_STATUS_ERR_INVALID_ARGUMENT;
    if (obj->w == 0u || obj->h == 0u) return GUIWIRE_STATUS_OK;

    if ((obj->flags & GUIWIRE_RECT_FLAG_FILL_TRANSPARENT) == 0u) {
        st = guiwire_server_fill_bg(server, obj->x, obj->y, obj->w, obj->h, obj->fill);
        if (st != GUIWIRE_STATUS_OK) return st;
    }

    if ((obj->flags & GUIWIRE_RECT_FLAG_BORDER_ENABLED) != 0u) {
        if (obj->w == 1u || obj->h == 1u) {
            st = guiwire_server_fill_bg(server, obj->x, obj->y, obj->w, obj->h, obj->border);
            if (st != GUIWIRE_STATUS_OK) return st;
        } else {
            st = guiwire_server_fill_bg(server, obj->x, obj->y, obj->w, 1u, obj->border);
            if (st != GUIWIRE_STATUS_OK) return st;

            st = guiwire_server_fill_bg(server, obj->x, (uint16_t)(obj->y + obj->h - 1u), obj->w, 1u, obj->border);
            if (st != GUIWIRE_STATUS_OK) return st;

            st = guiwire_server_fill_bg(server, obj->x, obj->y, 1u, obj->h, obj->border);
            if (st != GUIWIRE_STATUS_OK) return st;

            st = guiwire_server_fill_bg(server, (uint16_t)(obj->x + obj->w - 1u), obj->y, 1u, obj->h, obj->border);
            if (st != GUIWIRE_STATUS_OK) return st;
        }
    }

    return GUIWIRE_STATUS_OK;
}

guiwire_status_t guiwire_object_rect_create(guiwire_server_t *server, const uint8_t *payload, uint16_t payload_len) {
    guiwire_rect_desc_t desc;
    guiwire_rect_object_t *obj;
    guiwire_status_t st;

    st = parse_rect_desc(payload, payload_len, &desc);
    if (st != GUIWIRE_STATUS_OK) return st;

    obj = guiwire_server_find_rect_object(server, desc.object_id);
    if (obj) return GUIWIRE_STATUS_OK;

    st = create_or_replace_rect(server, &desc, 0);
    if (st != GUIWIRE_STATUS_OK) return st;

    obj = guiwire_server_find_rect_object(server, desc.object_id);
    st = guiwire_object_rect_draw(server, obj);
    if (st != GUIWIRE_STATUS_OK) return st;

    return guiwire_server_flush(server);
}

guiwire_status_t guiwire_object_rect_update(guiwire_server_t *server, const uint8_t *payload, uint16_t payload_len) {
    guiwire_rect_update_t update;
    guiwire_rect_object_t *obj;
    guiwire_status_t st;

    st = parse_rect_update(payload, payload_len, &update);
    if (st != GUIWIRE_STATUS_OK) return st;

    obj = guiwire_server_find_rect_object(server, update.object_id);
    if (!obj) return GUIWIRE_STATUS_ERR_NOT_FOUND;

    if (obj->fill == update.fill && obj->border == update.border && obj->flags == update.flags) {
        return GUIWIRE_STATUS_OK;
    }

    if ((update.flags & GUIWIRE_RECT_FLAG_FILL_TRANSPARENT) != 0u ||
        (obj->flags & GUIWIRE_RECT_FLAG_FILL_TRANSPARENT) != 0u) {
        st = clear_rect_object(server, obj);
        if (st != GUIWIRE_STATUS_OK) return st;
    }

    obj->fill = update.fill;
    obj->border = update.border;
    obj->flags = update.flags;

    st = guiwire_object_rect_draw(server, obj);
    if (st != GUIWIRE_STATUS_OK) return st;

    return guiwire_server_flush(server);
}

guiwire_status_t guiwire_object_rect_destroy(guiwire_server_t *server, guiwire_object_id_t object_id) {
    guiwire_rect_object_t *obj;
    guiwire_status_t st;

    obj = guiwire_server_find_rect_object(server, object_id);
    if (!obj) return GUIWIRE_STATUS_ERR_NOT_FOUND;

    st = clear_rect_object(server, obj);
    if (st != GUIWIRE_STATUS_OK) return st;

    memset(obj, 0, sizeof(*obj));

    return guiwire_server_flush(server);
}
