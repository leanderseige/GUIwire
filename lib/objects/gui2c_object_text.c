
#include <string.h>
#include "lib/objects/gui2c_object_text.h"
#include "lib/internal/gui2c_protocol_internal.h"
#include "lib/server/gui2c_server_objects.h"
#include "lib/renderer/gui2c_renderer_text.h"

static gui2c_status_t parse_text_desc(const uint8_t *payload, uint16_t payload_len, gui2c_text_desc_t *out_desc, char *text_buf, size_t text_buf_len) {
    uint8_t text_len;
    if (!payload || !out_desc || !text_buf) return GUI2C_STATUS_ERR_INVALID_ARGUMENT;
    if (payload_len < 17u) return GUI2C_STATUS_ERR_PROTOCOL;
    text_len = payload[16];
    if ((uint16_t)(17u + text_len) != payload_len) return GUI2C_STATUS_ERR_PROTOCOL;
    if ((size_t)text_len >= text_buf_len) return GUI2C_STATUS_ERR_BUFFER_TOO_SMALL;
    out_desc->object_id = gui2c_read_u16le(&payload[0]);
    out_desc->x = gui2c_read_u16le(&payload[2]);
    out_desc->y = gui2c_read_u16le(&payload[4]);
    out_desc->w = gui2c_read_u16le(&payload[6]);
    out_desc->h = gui2c_read_u16le(&payload[8]);
    out_desc->fg = gui2c_read_u16le(&payload[10]);
    out_desc->bg = gui2c_read_u16le(&payload[12]);
    out_desc->flags = payload[14];
    out_desc->zoom = payload[15] ? payload[15] : 1u;
    memcpy(text_buf, &payload[17], text_len);
    text_buf[text_len] = '\0';
    out_desc->text = text_buf;
    return GUI2C_STATUS_OK;
}

static gui2c_status_t clear_text_object(gui2c_server_t *server, const gui2c_text_object_t *obj) {
    return gui2c_server_fill_bg(server, obj->x, obj->y, obj->w, obj->h, obj->bg);
}

static gui2c_status_t create_or_replace_text(gui2c_server_t *server, const gui2c_text_desc_t *desc, int replace_existing) {
    gui2c_text_object_t *obj;
    if (!server || !desc || !desc->text) return GUI2C_STATUS_ERR_INVALID_ARGUMENT;
    obj = gui2c_server_find_text_object(server, desc->object_id);
    if (obj && !replace_existing) return GUI2C_STATUS_ERR_ALREADY_EXISTS;
    if (!obj) obj = gui2c_server_alloc_text_object(server, desc->object_id);
    if (!obj) return GUI2C_STATUS_ERR_NO_SPACE;
    obj->x = desc->x; obj->y = desc->y; obj->w = desc->w; obj->h = desc->h;
    obj->fg = desc->fg; obj->bg = desc->bg; obj->flags = desc->flags; obj->zoom = desc->zoom ? desc->zoom : 1u;
    strncpy(obj->text, desc->text, GUI2C_MAX_TEXT_LEN); obj->text[GUI2C_MAX_TEXT_LEN] = '\0';
    return GUI2C_STATUS_OK;
}

gui2c_status_t gui2c_object_text_draw(gui2c_server_t *server, const gui2c_text_object_t *obj) {
    size_t i, len;
    if (!server || !obj || !obj->in_use) return GUI2C_STATUS_ERR_INVALID_ARGUMENT;
    if ((obj->flags & GUI2C_TEXT_FLAG_BG_TRANSPARENT) == 0u) {
        gui2c_status_t st = gui2c_server_fill_bg(server, obj->x, obj->y, obj->w, obj->h, obj->bg);
        if (st != GUI2C_STATUS_OK) return st;
    }
    len = strlen(obj->text);
    for (i = 0; i < len; ++i) {
        uint16_t base_x = (uint16_t)(obj->x + i * 6u * obj->zoom);
        if (base_x >= (uint16_t)(obj->x + obj->w)) break;
        { gui2c_status_t st = gui2c_renderer_draw_glyph_5x7_clipped(server->cfg.renderer, server->cfg.renderer_ctx, obj->x, obj->y, obj->w, obj->h, base_x, obj->y, obj->zoom, (unsigned char)obj->text[i], obj->fg); if (st != GUI2C_STATUS_OK) return st; }
    }
    return GUI2C_STATUS_OK;
}

static gui2c_status_t redraw_changed_cells(gui2c_server_t *server, const gui2c_text_object_t *old_obj, const gui2c_text_object_t *new_obj) {
    size_t old_len, new_len, max_len, i;
    if (!server || !new_obj) return GUI2C_STATUS_ERR_INVALID_ARGUMENT;
    if (!old_obj || old_obj->x != new_obj->x || old_obj->y != new_obj->y || old_obj->w != new_obj->w || old_obj->h != new_obj->h || old_obj->fg != new_obj->fg || old_obj->bg != new_obj->bg || old_obj->flags != new_obj->flags || old_obj->zoom != new_obj->zoom) {
        return gui2c_object_text_draw(server, new_obj);
    }
    old_len = strlen(old_obj->text); new_len = strlen(new_obj->text); max_len = old_len > new_len ? old_len : new_len;
    for (i = 0; i < max_len; ++i) {
        char old_ch = (i < old_len) ? old_obj->text[i] : ' ';
        char new_ch = (i < new_len) ? new_obj->text[i] : ' ';
        if (old_ch != new_ch) {
            uint16_t base_x = (uint16_t)(new_obj->x + i * 6u * new_obj->zoom);
            if ((new_obj->flags & GUI2C_TEXT_FLAG_BG_TRANSPARENT) == 0u) {
                gui2c_status_t st = gui2c_server_fill_bg(server, base_x, new_obj->y, (uint16_t)(6u * new_obj->zoom), (uint16_t)(8u * new_obj->zoom), new_obj->bg);
                if (st != GUI2C_STATUS_OK) return st;
            }
            if (new_ch != ' ') {
                gui2c_status_t st = gui2c_renderer_draw_glyph_5x7_clipped(server->cfg.renderer, server->cfg.renderer_ctx, new_obj->x, new_obj->y, new_obj->w, new_obj->h, base_x, new_obj->y, new_obj->zoom, (unsigned char)new_ch, new_obj->fg);
                if (st != GUI2C_STATUS_OK) return st;
            }
        }
    }
    return GUI2C_STATUS_OK;
}

gui2c_status_t gui2c_object_text_create(gui2c_server_t *server, const uint8_t *payload, uint16_t payload_len) {
    gui2c_text_desc_t desc; char text_buf[GUI2C_MAX_TEXT_LEN + 1]; gui2c_text_object_t *obj; gui2c_status_t st;
    st = parse_text_desc(payload, payload_len, &desc, text_buf, sizeof(text_buf)); if (st != GUI2C_STATUS_OK) return st;
    st = create_or_replace_text(server, &desc, 0); if (st != GUI2C_STATUS_OK) return st;
    obj = gui2c_server_find_text_object(server, desc.object_id);
    st = gui2c_object_text_draw(server, obj); if (st != GUI2C_STATUS_OK) return st;
    return gui2c_server_flush(server);
}

gui2c_status_t gui2c_object_text_update(gui2c_server_t *server, const uint8_t *payload, uint16_t payload_len) {
    gui2c_text_desc_t desc; char text_buf[GUI2C_MAX_TEXT_LEN + 1]; gui2c_text_object_t old_copy; gui2c_text_object_t *obj; int had_old = 0; gui2c_status_t st;
    st = parse_text_desc(payload, payload_len, &desc, text_buf, sizeof(text_buf)); if (st != GUI2C_STATUS_OK) return st;
    obj = gui2c_server_find_text_object(server, desc.object_id); if (obj) { old_copy = *obj; had_old = 1; }
    st = create_or_replace_text(server, &desc, 1); if (st != GUI2C_STATUS_OK) return st;
    obj = gui2c_server_find_text_object(server, desc.object_id);
    st = redraw_changed_cells(server, had_old ? &old_copy : NULL, obj); if (st != GUI2C_STATUS_OK) return st;
    return gui2c_server_flush(server);
}

gui2c_status_t gui2c_object_text_destroy(gui2c_server_t *server, gui2c_object_id_t object_id) {
    gui2c_text_object_t *obj; gui2c_status_t st;
    obj = gui2c_server_find_text_object(server, object_id); if (!obj) return GUI2C_STATUS_ERR_NOT_FOUND;
    st = clear_text_object(server, obj); if (st != GUI2C_STATUS_OK) return st;
    memset(obj, 0, sizeof(*obj));
    return gui2c_server_flush(server);
}
