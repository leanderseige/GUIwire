/*
 * SPDX-License-Identifier: MIT
 * Authors: any:frequency, seige.digital GbR, Leander Seige (2026)
 */

#include <string.h>

#include "lib/objects/guiwire_object_bar.h"
#include "guiwire_protocol_internal.h"
#include "lib/server/guiwire_server_objects.h"

static guiwire_status_t parse_bar_desc(const uint8_t *payload, uint16_t payload_len, guiwire_bar_desc_t *out_desc) {
    if (!payload || !out_desc) return GUIWIRE_STATUS_ERR_INVALID_ARGUMENT;
    if (payload_len != 17u) return GUIWIRE_STATUS_ERR_PROTOCOL;

    out_desc->object_id = guiwire_read_u16le(&payload[0]);
    out_desc->x = guiwire_read_u16le(&payload[2]);
    out_desc->y = guiwire_read_u16le(&payload[4]);
    out_desc->w = guiwire_read_u16le(&payload[6]);
    out_desc->h = guiwire_read_u16le(&payload[8]);
    out_desc->fg = guiwire_read_u16le(&payload[10]);
    out_desc->bg = guiwire_read_u16le(&payload[12]);
    out_desc->value = guiwire_read_u16le(&payload[14]);
    out_desc->direction = payload[16];

    if (out_desc->direction > (uint8_t)GUIWIRE_BAR_DIRECTION_RIGHT_TO_LEFT) {
        return GUIWIRE_STATUS_ERR_PROTOCOL;
    }

    return GUIWIRE_STATUS_OK;
}

static guiwire_status_t parse_bar_update(const uint8_t *payload, uint16_t payload_len, guiwire_bar_update_t *out_update) {
    if (!payload || !out_update) return GUIWIRE_STATUS_ERR_INVALID_ARGUMENT;
    if (payload_len != 4u) return GUIWIRE_STATUS_ERR_PROTOCOL;

    out_update->object_id = guiwire_read_u16le(&payload[0]);
    out_update->value = guiwire_read_u16le(&payload[2]);
    return GUIWIRE_STATUS_OK;
}

static guiwire_status_t clear_bar_object(guiwire_server_t *server, const guiwire_bar_object_t *obj) {
    if (!server || !obj) return GUIWIRE_STATUS_ERR_INVALID_ARGUMENT;
    return guiwire_server_fill_bg(server, obj->x, obj->y, obj->w, obj->h, obj->bg);
}

static guiwire_status_t create_or_replace_bar(guiwire_server_t *server, const guiwire_bar_desc_t *desc, int replace_existing) {
    guiwire_bar_object_t *obj;

    if (!server || !desc) return GUIWIRE_STATUS_ERR_INVALID_ARGUMENT;

    obj = guiwire_server_find_bar_object(server, desc->object_id);
    if (obj && !replace_existing) return GUIWIRE_STATUS_ERR_ALREADY_EXISTS;
    if (!obj) obj = guiwire_server_alloc_bar_object(server, desc->object_id);
    if (!obj) return GUIWIRE_STATUS_ERR_NO_SPACE;

    obj->x = desc->x;
    obj->y = desc->y;
    obj->w = desc->w;
    obj->h = desc->h;
    obj->fg = desc->fg;
    obj->bg = desc->bg;
    obj->value = desc->value;
    obj->direction = desc->direction;

    return GUIWIRE_STATUS_OK;
}

static uint16_t bar_filled_extent(uint16_t value, uint16_t extent) {
    uint32_t scaled;

    if (value == 0u || extent == 0u) return 0u;
    if (value == 0xFFFFu) return extent;

    scaled = ((uint32_t)value * (uint32_t)extent + 65534u) / 65535u;
    if (scaled == 0u) scaled = 1u;
    if (scaled > extent) scaled = extent;
    return (uint16_t)scaled;
}

static guiwire_status_t bar_draw_delta(guiwire_server_t *server,
                                     const guiwire_bar_object_t *obj,
                                     uint16_t old_filled,
                                     uint16_t new_filled) {
    if (!server || !obj) return GUIWIRE_STATUS_ERR_INVALID_ARGUMENT;
    if (old_filled == new_filled) return GUIWIRE_STATUS_OK;

    if (new_filled > old_filled) {
        uint16_t delta = (uint16_t)(new_filled - old_filled);

        switch ((guiwire_bar_direction_t)obj->direction) {
            case GUIWIRE_BAR_DIRECTION_TOP_TO_BOTTOM:
                return guiwire_server_fill_bg(server, obj->x, (uint16_t)(obj->y + old_filled), obj->w, delta, obj->fg);
            case GUIWIRE_BAR_DIRECTION_BOTTOM_TO_TOP:
                return guiwire_server_fill_bg(server, obj->x, (uint16_t)(obj->y + obj->h - new_filled), obj->w, delta, obj->fg);
            case GUIWIRE_BAR_DIRECTION_RIGHT_TO_LEFT:
                return guiwire_server_fill_bg(server, (uint16_t)(obj->x + obj->w - new_filled), obj->y, delta, obj->h, obj->fg);
            case GUIWIRE_BAR_DIRECTION_LEFT_TO_RIGHT:
            default:
                return guiwire_server_fill_bg(server, (uint16_t)(obj->x + old_filled), obj->y, delta, obj->h, obj->fg);
        }
    } else {
        uint16_t delta = (uint16_t)(old_filled - new_filled);

        switch ((guiwire_bar_direction_t)obj->direction) {
            case GUIWIRE_BAR_DIRECTION_TOP_TO_BOTTOM:
                return guiwire_server_fill_bg(server, obj->x, (uint16_t)(obj->y + new_filled), obj->w, delta, obj->bg);
            case GUIWIRE_BAR_DIRECTION_BOTTOM_TO_TOP:
                return guiwire_server_fill_bg(server, obj->x, (uint16_t)(obj->y + obj->h - old_filled), obj->w, delta, obj->bg);
            case GUIWIRE_BAR_DIRECTION_RIGHT_TO_LEFT:
                return guiwire_server_fill_bg(server, (uint16_t)(obj->x + obj->w - old_filled), obj->y, delta, obj->h, obj->bg);
            case GUIWIRE_BAR_DIRECTION_LEFT_TO_RIGHT:
            default:
                return guiwire_server_fill_bg(server, (uint16_t)(obj->x + new_filled), obj->y, delta, obj->h, obj->bg);
        }
    }
}

guiwire_status_t guiwire_object_bar_draw(guiwire_server_t *server, const guiwire_bar_object_t *obj) {
    guiwire_status_t st;
    uint16_t filled;
    uint16_t draw_x;
    uint16_t draw_y;
    uint16_t draw_w;
    uint16_t draw_h;

    if (!server || !obj || !obj->in_use) return GUIWIRE_STATUS_ERR_INVALID_ARGUMENT;
    if (obj->w == 0u || obj->h == 0u) return GUIWIRE_STATUS_OK;

    st = guiwire_server_fill_bg(server, obj->x, obj->y, obj->w, obj->h, obj->bg);
    if (st != GUIWIRE_STATUS_OK) return st;

    draw_x = obj->x;
    draw_y = obj->y;
    draw_w = obj->w;
    draw_h = obj->h;

    switch ((guiwire_bar_direction_t)obj->direction) {
        case GUIWIRE_BAR_DIRECTION_TOP_TO_BOTTOM:
            filled = bar_filled_extent(obj->value, obj->h);
            draw_h = filled;
            break;
        case GUIWIRE_BAR_DIRECTION_BOTTOM_TO_TOP:
            filled = bar_filled_extent(obj->value, obj->h);
            draw_y = (uint16_t)(obj->y + obj->h - filled);
            draw_h = filled;
            break;
        case GUIWIRE_BAR_DIRECTION_RIGHT_TO_LEFT:
            filled = bar_filled_extent(obj->value, obj->w);
            draw_x = (uint16_t)(obj->x + obj->w - filled);
            draw_w = filled;
            break;
        case GUIWIRE_BAR_DIRECTION_LEFT_TO_RIGHT:
        default:
            filled = bar_filled_extent(obj->value, obj->w);
            draw_w = filled;
            break;
    }

    if (filled == 0u) return GUIWIRE_STATUS_OK;
    return guiwire_server_fill_bg(server, draw_x, draw_y, draw_w, draw_h, obj->fg);
}

guiwire_status_t guiwire_object_bar_create(guiwire_server_t *server, const uint8_t *payload, uint16_t payload_len) {
    guiwire_bar_desc_t desc;
    guiwire_bar_object_t *obj;
    guiwire_status_t st;

    st = parse_bar_desc(payload, payload_len, &desc);
    if (st != GUIWIRE_STATUS_OK) return st;

    obj = guiwire_server_find_bar_object(server, desc.object_id);
    if (obj) return GUIWIRE_STATUS_OK;

    st = create_or_replace_bar(server, &desc, 0);
    if (st != GUIWIRE_STATUS_OK) return st;

    obj = guiwire_server_find_bar_object(server, desc.object_id);
    st = guiwire_object_bar_draw(server, obj);
    if (st != GUIWIRE_STATUS_OK) return st;

    return guiwire_server_flush(server);
}

guiwire_status_t guiwire_object_bar_update(guiwire_server_t *server, const uint8_t *payload, uint16_t payload_len) {
    guiwire_bar_update_t update;
    guiwire_bar_object_t *obj;
    guiwire_status_t st;
    uint16_t extent;
    uint16_t old_filled;
    uint16_t new_filled;

    st = parse_bar_update(payload, payload_len, &update);
    if (st != GUIWIRE_STATUS_OK) return st;

    obj = guiwire_server_find_bar_object(server, update.object_id);
    if (!obj) return GUIWIRE_STATUS_ERR_NOT_FOUND;

    extent = ((guiwire_bar_direction_t)obj->direction == GUIWIRE_BAR_DIRECTION_TOP_TO_BOTTOM ||
              (guiwire_bar_direction_t)obj->direction == GUIWIRE_BAR_DIRECTION_BOTTOM_TO_TOP) ? obj->h : obj->w;
    old_filled = bar_filled_extent(obj->value, extent);
    obj->value = update.value;
    new_filled = bar_filled_extent(obj->value, extent);

    st = bar_draw_delta(server, obj, old_filled, new_filled);
    if (st != GUIWIRE_STATUS_OK) return st;

    return guiwire_server_flush(server);
}

guiwire_status_t guiwire_object_bar_destroy(guiwire_server_t *server, guiwire_object_id_t object_id) {
    guiwire_bar_object_t *obj;
    guiwire_status_t st;

    obj = guiwire_server_find_bar_object(server, object_id);
    if (!obj) return GUIWIRE_STATUS_ERR_NOT_FOUND;

    st = clear_bar_object(server, obj);
    if (st != GUIWIRE_STATUS_OK) return st;

    memset(obj, 0, sizeof(*obj));
    return guiwire_server_flush(server);
}
