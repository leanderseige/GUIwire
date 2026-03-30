#include <string.h>
#include "gui2c/gui2c.h"
#include "lib/internal/gui2c_protocol_internal.h"
#include "lib/server/gui2c_server_objects.h"
#include "lib/objects/gui2c_object_text.h"
#include "lib/objects/gui2c_object_rect.h"
#include "lib/objects/gui2c_object_line.h"
#include "lib/objects/gui2c_object_bar.h"
#include "lib/objects/gui2c_object_ring.h"
#include "lib/objects/gui2c_object_waveform.h"

gui2c_color_t gui2c_rgb565(uint8_t r, uint8_t g, uint8_t b) {
    return (gui2c_color_t)(((r & 0xF8u) << 8) | ((g & 0xFCu) << 3) | ((b & 0xF8u) >> 3));
}

gui2c_status_t gui2c_server_init(gui2c_server_t *server, const gui2c_server_config_t *cfg) {
    gui2c_text_object_t *boot;
    gui2c_status_t st;
    if (!server || !cfg || !cfg->renderer || !cfg->renderer->fill_rect || !cfg->renderer->draw_pixel) return GUI2C_STATUS_ERR_INVALID_ARGUMENT;
    memset(server, 0, sizeof(*server)); server->cfg = *cfg;
    st = gui2c_server_fill_bg(server, 0u, 0u, server->cfg.display_size.width, server->cfg.display_size.height, gui2c_rgb565(0,0,0));
    if (st != GUI2C_STATUS_OK) return st;
    st = gui2c_server_flush(server); if (st != GUI2C_STATUS_OK) return st;
    boot = gui2c_server_alloc_text_object(server, 0u); if (!boot) return GUI2C_STATUS_ERR_NO_SPACE;
    boot->x = 8u; boot->y = 8u; boot->w = 220u; boot->h = 10u; boot->fg = gui2c_rgb565(0,255,0); boot->bg = gui2c_rgb565(0,0,0); boot->flags = 0u; boot->zoom = 1u;
    strncpy(boot->text, cfg->server_id ? cfg->server_id : "GUI2C SERVER", GUI2C_MAX_TEXT_LEN); boot->text[GUI2C_MAX_TEXT_LEN] = '\0';
    return GUI2C_STATUS_OK;
}

gui2c_status_t gui2c_server_show_boot(gui2c_server_t *server) {
    size_t i; gui2c_status_t st;
    if (!server) return GUI2C_STATUS_ERR_INVALID_ARGUMENT;
    st = gui2c_server_fill_bg(server, 0u, 0u, server->cfg.display_size.width, server->cfg.display_size.height, gui2c_rgb565(0,0,0));
    if (st != GUI2C_STATUS_OK) return st;
    for (i = 0; i < GUI2C_MAX_OBJECTS; ++i) {
        if (server->rect_objects[i].in_use) {
            st = gui2c_object_rect_draw(server, &server->rect_objects[i]);
            if (st != GUI2C_STATUS_OK) return st;
        }
    }
    for (i = 0; i < GUI2C_MAX_OBJECTS; ++i) {
        if (server->bar_objects[i].in_use) {
            st = gui2c_object_bar_draw(server, &server->bar_objects[i]);
            if (st != GUI2C_STATUS_OK) return st;
        }
    }
    for (i = 0; i < GUI2C_MAX_OBJECTS; ++i) {
        if (server->ring_objects[i].in_use) {
            st = gui2c_object_ring_draw(server, &server->ring_objects[i]);
            if (st != GUI2C_STATUS_OK) return st;
        }
    }
    for (i = 0; i < GUI2C_SERVER_MAX_WAVEFORM_OBJECTS; ++i) {
        if (server->waveform_objects[i].in_use) {
            st = gui2c_object_waveform_draw(server, &server->waveform_objects[i]);
            if (st != GUI2C_STATUS_OK) return st;
        }
    }
    for (i = 0; i < GUI2C_MAX_OBJECTS; ++i) {
        if (server->line_objects[i].in_use) {
            st = gui2c_object_line_draw(server, &server->line_objects[i], server->line_objects[i].fg);
            if (st != GUI2C_STATUS_OK) return st;
        }
    }
    for (i = 0; i < GUI2C_MAX_OBJECTS; ++i) {
        if (server->text_objects[i].in_use) {
            st = gui2c_object_text_draw(server, &server->text_objects[i]);
            if (st != GUI2C_STATUS_OK) return st;
        }
    }
    return gui2c_server_flush(server);
}

gui2c_status_t gui2c_server_show_waiting(gui2c_server_t *server) {
    gui2c_status_t st; gui2c_text_object_t *obj = gui2c_server_find_text_object(server, 1u);
    if (!server) return GUI2C_STATUS_ERR_INVALID_ARGUMENT;
    if (!obj) obj = gui2c_server_alloc_text_object(server, 1u);
    if (!obj) return GUI2C_STATUS_ERR_NO_SPACE;
    obj->x = 8u; obj->y = 24u; obj->w = 200u; obj->h = 10u; obj->fg = gui2c_rgb565(255,255,0); obj->bg = gui2c_rgb565(0,0,0); obj->flags = 0u; obj->zoom = 1u;
    strncpy(obj->text, "Waiting for client...", GUI2C_MAX_TEXT_LEN); obj->text[GUI2C_MAX_TEXT_LEN] = '\0';
    st = gui2c_object_text_draw(server, obj); if (st != GUI2C_STATUS_OK) return st;
    return gui2c_server_flush(server);
}

gui2c_status_t gui2c_server_redraw(gui2c_server_t *server) { return gui2c_server_show_boot(server); }

gui2c_status_t gui2c_server_handle_packet(gui2c_server_t *server, const uint8_t *packet, size_t packet_len) {
    gui2c_packet_view_t view; gui2c_status_t st;
    if (!server || !packet) return GUI2C_STATUS_ERR_INVALID_ARGUMENT;
    st = gui2c_protocol_parse(packet, packet_len, &view); if (st != GUI2C_STATUS_OK) return st;
    switch ((gui2c_command_t)view.header.command) {
        case GUI2C_CMD_PING:
            return GUI2C_STATUS_OK;
        case GUI2C_CMD_HELLO_STRING: {
            size_t n = view.header.payload_len;
            if (n > GUI2C_MAX_TEXT_LEN) return GUI2C_STATUS_ERR_PROTOCOL;
            memcpy(server->last_client_string, view.payload, n);
            server->last_client_string[n] = '\0';
            return GUI2C_STATUS_OK;
        }
        case GUI2C_CMD_DESTROY_ALL_OBJECTS:
            gui2c_server_clear_all_objects(server);
            st = gui2c_server_fill_bg(server, 0u, 0u, server->cfg.display_size.width, server->cfg.display_size.height, gui2c_rgb565(0,0,0));
            if (st != GUI2C_STATUS_OK) return st;
            return gui2c_server_flush(server);
        case GUI2C_CMD_CREATE_TEXT:
            return gui2c_object_text_create(server, view.payload, view.header.payload_len);
        case GUI2C_CMD_UPDATE_TEXT:
            return gui2c_object_text_update(server, view.payload, view.header.payload_len);
        case GUI2C_CMD_CREATE_RECT:
            return gui2c_object_rect_create(server, view.payload, view.header.payload_len);
        case GUI2C_CMD_UPDATE_RECT:
            return gui2c_object_rect_update(server, view.payload, view.header.payload_len);
        case GUI2C_CMD_CREATE_LINE:
            return gui2c_object_line_create(server, view.payload, view.header.payload_len);
        case GUI2C_CMD_UPDATE_LINE:
            return gui2c_object_line_update(server, view.payload, view.header.payload_len);
        case GUI2C_CMD_CREATE_BAR:
            return gui2c_object_bar_create(server, view.payload, view.header.payload_len);
        case GUI2C_CMD_UPDATE_BAR:
            return gui2c_object_bar_update(server, view.payload, view.header.payload_len);
        case GUI2C_CMD_CREATE_RING:
            return gui2c_object_ring_create(server, view.payload, view.header.payload_len);
        case GUI2C_CMD_UPDATE_RING:
            return gui2c_object_ring_update(server, view.payload, view.header.payload_len);
        case GUI2C_CMD_CREATE_WAVEFORM:
            return gui2c_object_waveform_create(server, view.payload, view.header.payload_len);
        case GUI2C_CMD_UPDATE_WAVEFORM:
            return gui2c_object_waveform_update(server, view.payload, view.header.payload_len);
        case GUI2C_CMD_DESTROY_OBJECT: {
            gui2c_object_id_t object_id;
            if (view.header.payload_len != 2u) return GUI2C_STATUS_ERR_PROTOCOL;
            object_id = gui2c_read_u16le(view.payload);

            st = gui2c_object_text_destroy(server, object_id);
            if (st == GUI2C_STATUS_OK) return st;

            st = gui2c_object_rect_destroy(server, object_id);
            if (st == GUI2C_STATUS_OK) return st;

            st = gui2c_object_line_destroy(server, object_id);
            if (st == GUI2C_STATUS_OK) return st;

            st = gui2c_object_bar_destroy(server, object_id);
            if (st == GUI2C_STATUS_OK) return st;

            st = gui2c_object_ring_destroy(server, object_id);
            if (st == GUI2C_STATUS_OK) return st;

            st = gui2c_object_waveform_destroy(server, object_id);
            if (st == GUI2C_STATUS_OK) return st;

            return GUI2C_STATUS_ERR_NOT_FOUND;
        }
        default:
            return GUI2C_STATUS_ERR_NOT_SUPPORTED;
    }
}
