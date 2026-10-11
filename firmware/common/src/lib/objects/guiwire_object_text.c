/*
 * SPDX-License-Identifier: MIT
 * Authors: any:frequency, seige.digital GbR, Leander Seige (2026)
 */


#include <string.h>
#include "lib/objects/guiwire_object_text.h"
#include "guiwire_protocol_internal.h"
#include "lib/server/guiwire_server_objects.h"
#include "lib/renderer/guiwire_renderer_text.h"

#define GUIWIRE_TEXT_CELL_WIDTH   6u

static uint16_t text_padding(const guiwire_text_object_t *obj) {
    return (uint16_t)(obj->zoom ? obj->zoom : 1u);
}

static uint16_t text_content_x(const guiwire_text_object_t *obj) {
    return (uint16_t)(obj->x + text_padding(obj));
}

static uint16_t text_content_y(const guiwire_text_object_t *obj) {
    return (uint16_t)(obj->y + text_padding(obj));
}

static uint16_t text_content_w(const guiwire_text_object_t *obj) {
    uint16_t padding = text_padding(obj);
    return (obj->w > padding) ? (uint16_t)(obj->w - padding) : 0u;
}

static uint16_t text_content_h(const guiwire_text_object_t *obj) {
    uint16_t padding = text_padding(obj);
    uint16_t vertical_padding = (uint16_t)(padding * 2u);
    return (obj->h > vertical_padding) ? (uint16_t)(obj->h - vertical_padding) : 0u;
}

static guiwire_status_t parse_text_desc(const uint8_t *payload, uint16_t payload_len, guiwire_text_desc_t *out_desc, char *text_buf, size_t text_buf_len) {
    uint8_t text_len;
    if (!payload || !out_desc || !text_buf) return GUIWIRE_STATUS_ERR_INVALID_ARGUMENT;
    if (payload_len < 17u) return GUIWIRE_STATUS_ERR_PROTOCOL;
    text_len = payload[16];
    if ((uint16_t)(17u + text_len) != payload_len) return GUIWIRE_STATUS_ERR_PROTOCOL;
    if ((size_t)text_len >= text_buf_len) return GUIWIRE_STATUS_ERR_BUFFER_TOO_SMALL;
    out_desc->object_id = guiwire_read_u16le(&payload[0]);
    out_desc->x = guiwire_read_u16le(&payload[2]);
    out_desc->y = guiwire_read_u16le(&payload[4]);
    out_desc->w = guiwire_read_u16le(&payload[6]);
    out_desc->h = guiwire_read_u16le(&payload[8]);
    out_desc->fg = guiwire_read_u16le(&payload[10]);
    out_desc->bg = guiwire_read_u16le(&payload[12]);
    out_desc->flags = payload[14];
    out_desc->zoom = payload[15] ? payload[15] : 1u;
    memcpy(text_buf, &payload[17], text_len);
    text_buf[text_len] = '\0';
    out_desc->text = text_buf;
    return GUIWIRE_STATUS_OK;
}

static guiwire_status_t parse_text_update(const uint8_t *payload,
                                        uint16_t payload_len,
                                        guiwire_text_update_t *out_update,
                                        char *text_buf,
                                        size_t text_buf_len) {
    uint8_t text_len;

    if (!payload || !out_update || !text_buf) return GUIWIRE_STATUS_ERR_INVALID_ARGUMENT;
    if (payload_len < 9u) return GUIWIRE_STATUS_ERR_PROTOCOL;

    text_len = payload[8];
    if ((uint16_t)(9u + text_len) != payload_len) return GUIWIRE_STATUS_ERR_PROTOCOL;
    if ((size_t)text_len >= text_buf_len) return GUIWIRE_STATUS_ERR_BUFFER_TOO_SMALL;

    out_update->object_id = guiwire_read_u16le(&payload[0]);
    out_update->fg = guiwire_read_u16le(&payload[2]);
    out_update->bg = guiwire_read_u16le(&payload[4]);
    out_update->flags = payload[6];
    out_update->zoom = payload[7] ? payload[7] : 1u;
    memcpy(text_buf, &payload[9], text_len);
    text_buf[text_len] = '\0';
    out_update->text = text_buf;
    return GUIWIRE_STATUS_OK;
}

static guiwire_status_t clear_text_object(guiwire_server_t *server, const guiwire_text_object_t *obj) {
    return guiwire_server_fill_bg(server, obj->x, obj->y, obj->w, obj->h, obj->bg);
}

static guiwire_status_t create_or_replace_text(guiwire_server_t *server, const guiwire_text_desc_t *desc, int replace_existing) {
    guiwire_text_object_t *obj;
    if (!server || !desc || !desc->text) return GUIWIRE_STATUS_ERR_INVALID_ARGUMENT;
    obj = guiwire_server_find_text_object(server, desc->object_id);
    if (obj && !replace_existing) return GUIWIRE_STATUS_ERR_ALREADY_EXISTS;
    if (!obj) obj = guiwire_server_alloc_text_object(server, desc->object_id);
    if (!obj) return GUIWIRE_STATUS_ERR_NO_SPACE;
    obj->x = desc->x; obj->y = desc->y; obj->w = desc->w; obj->h = desc->h;
    obj->fg = desc->fg; obj->bg = desc->bg; obj->flags = desc->flags; obj->zoom = desc->zoom ? desc->zoom : 1u;
    strncpy(obj->text, desc->text, GUIWIRE_MAX_TEXT_LEN); obj->text[GUIWIRE_MAX_TEXT_LEN] = '\0';
    return GUIWIRE_STATUS_OK;
}

guiwire_status_t guiwire_object_text_draw(guiwire_server_t *server, const guiwire_text_object_t *obj) {
    size_t i, len;
    uint16_t clip_x, clip_y, clip_w, clip_h;
    if (!server || !obj || !obj->in_use) return GUIWIRE_STATUS_ERR_INVALID_ARGUMENT;
    if ((obj->flags & GUIWIRE_TEXT_FLAG_BG_TRANSPARENT) == 0u) {
        guiwire_status_t st = guiwire_server_fill_bg(server, obj->x, obj->y, obj->w, obj->h, obj->bg);
        if (st != GUIWIRE_STATUS_OK) return st;
    }
    clip_x = text_content_x(obj);
    clip_y = text_content_y(obj);
    clip_w = text_content_w(obj);
    clip_h = text_content_h(obj);
    if (clip_w == 0u || clip_h == 0u) return GUIWIRE_STATUS_OK;
    len = strlen(obj->text);
    for (i = 0; i < len; ++i) {
        uint16_t base_x = (uint16_t)(clip_x + i * GUIWIRE_TEXT_CELL_WIDTH * obj->zoom);
        if (base_x >= (uint16_t)(clip_x + clip_w)) break;
        { guiwire_status_t st = guiwire_renderer_draw_glyph_5x7_clipped(server->cfg.renderer, server->cfg.renderer_ctx, clip_x, clip_y, clip_w, clip_h, base_x, clip_y, obj->zoom, (unsigned char)obj->text[i], obj->fg); if (st != GUIWIRE_STATUS_OK) return st; }
    }
    return GUIWIRE_STATUS_OK;
}

static guiwire_status_t redraw_changed_cells(guiwire_server_t *server, const guiwire_text_object_t *old_obj, const guiwire_text_object_t *new_obj) {
    size_t old_len, new_len, max_len, i;
    uint16_t clip_x, clip_y, clip_w, clip_h;
    if (!server || !new_obj) return GUIWIRE_STATUS_ERR_INVALID_ARGUMENT;
    if (!old_obj || old_obj->fg != new_obj->fg || old_obj->bg != new_obj->bg ||
        old_obj->flags != new_obj->flags || old_obj->zoom != new_obj->zoom) {
        return guiwire_object_text_draw(server, new_obj);
    }
    clip_x = text_content_x(new_obj);
    clip_y = text_content_y(new_obj);
    clip_w = text_content_w(new_obj);
    clip_h = text_content_h(new_obj);
    if (clip_w == 0u || clip_h == 0u) return GUIWIRE_STATUS_OK;
    old_len = strlen(old_obj->text); new_len = strlen(new_obj->text); max_len = old_len > new_len ? old_len : new_len;
    for (i = 0; i < max_len; ++i) {
        char old_ch = (i < old_len) ? old_obj->text[i] : ' ';
        char new_ch = (i < new_len) ? new_obj->text[i] : ' ';
        if (old_ch != new_ch) {
            uint16_t base_x = (uint16_t)(clip_x + i * GUIWIRE_TEXT_CELL_WIDTH * new_obj->zoom);
            if (base_x >= (uint16_t)(clip_x + clip_w)) break;
            if ((new_obj->flags & GUIWIRE_TEXT_FLAG_BG_TRANSPARENT) == 0u) {
                guiwire_status_t st = guiwire_server_fill_bg(server, base_x, clip_y, (uint16_t)(GUIWIRE_TEXT_CELL_WIDTH * new_obj->zoom), clip_h, new_obj->bg);
                if (st != GUIWIRE_STATUS_OK) return st;
            }
            if (new_ch != ' ') {
                guiwire_status_t st = guiwire_renderer_draw_glyph_5x7_clipped(server->cfg.renderer, server->cfg.renderer_ctx, clip_x, clip_y, clip_w, clip_h, base_x, clip_y, new_obj->zoom, (unsigned char)new_ch, new_obj->fg);
                if (st != GUIWIRE_STATUS_OK) return st;
            }
        }
    }
    return GUIWIRE_STATUS_OK;
}

static int text_objects_equal(const guiwire_text_object_t *a, const guiwire_text_object_t *b) {
    if (!a || !b) return 0;
    return a->fg == b->fg &&
           a->bg == b->bg &&
           a->flags == b->flags &&
           a->zoom == b->zoom &&
           strcmp(a->text, b->text) == 0;
}

guiwire_status_t guiwire_object_text_create(guiwire_server_t *server, const uint8_t *payload, uint16_t payload_len) {
    guiwire_text_desc_t desc; char text_buf[GUIWIRE_MAX_TEXT_LEN + 1]; guiwire_text_object_t *obj; guiwire_status_t st;
    st = parse_text_desc(payload, payload_len, &desc, text_buf, sizeof(text_buf)); if (st != GUIWIRE_STATUS_OK) return st;
    obj = guiwire_server_find_text_object(server, desc.object_id);
    if (obj) return GUIWIRE_STATUS_OK;
    st = create_or_replace_text(server, &desc, 0); if (st != GUIWIRE_STATUS_OK) return st;
    obj = guiwire_server_find_text_object(server, desc.object_id);
    st = guiwire_object_text_draw(server, obj); if (st != GUIWIRE_STATUS_OK) return st;
    return guiwire_server_flush(server);
}

guiwire_status_t guiwire_object_text_update(guiwire_server_t *server, const uint8_t *payload, uint16_t payload_len) {
    guiwire_text_update_t update;
    char text_buf[GUIWIRE_MAX_TEXT_LEN + 1];
    guiwire_text_object_t old_copy;
    guiwire_text_object_t *obj;
    guiwire_status_t st;

    st = parse_text_update(payload, payload_len, &update, text_buf, sizeof(text_buf));
    if (st != GUIWIRE_STATUS_OK) return st;

    obj = guiwire_server_find_text_object(server, update.object_id);
    if (!obj) return GUIWIRE_STATUS_ERR_NOT_FOUND;

    old_copy = *obj;
    obj->fg = update.fg;
    obj->bg = update.bg;
    obj->flags = update.flags;
    obj->zoom = update.zoom ? update.zoom : 1u;
    strncpy(obj->text, update.text, GUIWIRE_MAX_TEXT_LEN);
    obj->text[GUIWIRE_MAX_TEXT_LEN] = '\0';

    if (text_objects_equal(&old_copy, obj)) return GUIWIRE_STATUS_OK;
    st = redraw_changed_cells(server, &old_copy, obj); if (st != GUIWIRE_STATUS_OK) return st;
    return guiwire_server_flush(server);
}

guiwire_status_t guiwire_object_text_destroy(guiwire_server_t *server, guiwire_object_id_t object_id) {
    guiwire_text_object_t *obj; guiwire_status_t st;
    obj = guiwire_server_find_text_object(server, object_id); if (!obj) return GUIWIRE_STATUS_ERR_NOT_FOUND;
    st = clear_text_object(server, obj); if (st != GUIWIRE_STATUS_OK) return st;
    memset(obj, 0, sizeof(*obj));
    return guiwire_server_flush(server);
}
