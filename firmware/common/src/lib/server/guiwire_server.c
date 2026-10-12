/*
 * SPDX-License-Identifier: MIT
 * Authors: any:frequency, seige.digital GbR, Leander Seige (2026)
 */

#include <string.h>
#include "guiwire/guiwire_server.h"
#include "guiwire_protocol_internal.h"
#include "lib/server/guiwire_server_objects.h"
#include "lib/objects/guiwire_object_text.h"
#include "lib/objects/guiwire_object_rect.h"
#include "lib/objects/guiwire_object_line.h"
#include "lib/objects/guiwire_object_bar.h"
#include "lib/objects/guiwire_object_ring.h"
#include "lib/objects/guiwire_object_waveform.h"

guiwire_status_t guiwire_server_init(guiwire_server_t *server, const guiwire_server_config_t *cfg) {
    guiwire_text_object_t *boot;
    guiwire_status_t st;
    if (!server || !cfg || !cfg->renderer || !cfg->renderer->fill_rect || !cfg->renderer->draw_pixel) return GUIWIRE_STATUS_ERR_INVALID_ARGUMENT;
    memset(server, 0, sizeof(*server)); server->cfg = *cfg;
    st = guiwire_server_fill_bg(server, 0u, 0u, server->cfg.display_size.width, server->cfg.display_size.height, guiwire_rgb565(0,0,0));
    if (st != GUIWIRE_STATUS_OK) return st;
    st = guiwire_server_flush(server); if (st != GUIWIRE_STATUS_OK) return st;
    boot = guiwire_server_alloc_text_object(server, 0u); if (!boot) return GUIWIRE_STATUS_ERR_NO_SPACE;
    boot->x = 8u; boot->y = 8u; boot->w = 220u; boot->h = 10u; boot->fg = guiwire_rgb565(0,255,0); boot->bg = guiwire_rgb565(0,0,0); boot->flags = 0u; boot->zoom = 1u;
    strncpy(boot->text, cfg->server_id ? cfg->server_id : "GUIWIRE SERVER", GUIWIRE_MAX_TEXT_LEN); boot->text[GUIWIRE_MAX_TEXT_LEN] = '\0';
    return GUIWIRE_STATUS_OK;
}

guiwire_status_t guiwire_server_show_boot(guiwire_server_t *server) {
    size_t i; guiwire_status_t st;
    if (!server) return GUIWIRE_STATUS_ERR_INVALID_ARGUMENT;
    st = guiwire_server_fill_bg(server, 0u, 0u, server->cfg.display_size.width, server->cfg.display_size.height, guiwire_rgb565(0,0,0));
    if (st != GUIWIRE_STATUS_OK) return st;
    for (i = 0; i < GUIWIRE_MAX_OBJECTS; ++i) {
        if (server->rect_objects[i].in_use) {
            st = guiwire_object_rect_draw(server, &server->rect_objects[i]);
            if (st != GUIWIRE_STATUS_OK) return st;
        }
    }
    for (i = 0; i < GUIWIRE_MAX_OBJECTS; ++i) {
        if (server->bar_objects[i].in_use) {
            st = guiwire_object_bar_draw(server, &server->bar_objects[i]);
            if (st != GUIWIRE_STATUS_OK) return st;
        }
    }
    for (i = 0; i < GUIWIRE_MAX_OBJECTS; ++i) {
        if (server->ring_objects[i].in_use) {
            st = guiwire_object_ring_draw(server, &server->ring_objects[i]);
            if (st != GUIWIRE_STATUS_OK) return st;
        }
    }
    for (i = 0; i < GUIWIRE_SERVER_MAX_WAVEFORM_OBJECTS; ++i) {
        if (server->waveform_objects[i].in_use) {
            st = guiwire_object_waveform_draw(server, &server->waveform_objects[i]);
            if (st != GUIWIRE_STATUS_OK) return st;
        }
    }
    for (i = 0; i < GUIWIRE_MAX_OBJECTS; ++i) {
        if (server->line_objects[i].in_use) {
            st = guiwire_object_line_draw(server, &server->line_objects[i], server->line_objects[i].fg);
            if (st != GUIWIRE_STATUS_OK) return st;
        }
    }
    for (i = 0; i < GUIWIRE_MAX_OBJECTS; ++i) {
        /* Object 0 is the transient Host status line (boot/waiting). It is
         * deliberately not part of a client scene redraw. */
        if (server->text_objects[i].in_use && server->text_objects[i].object_id != 0u) {
            st = guiwire_object_text_draw(server, &server->text_objects[i]);
            if (st != GUIWIRE_STATUS_OK) return st;
        }
    }
    return guiwire_server_flush(server);
}

guiwire_status_t guiwire_server_show_waiting(guiwire_server_t *server) {
    guiwire_status_t st;
    guiwire_text_object_t *obj;
    if (!server) return GUIWIRE_STATUS_ERR_INVALID_ARGUMENT;
    obj = guiwire_server_find_text_object(server, 0u);
    if (!obj) obj = guiwire_server_alloc_text_object(server, 0u);
    if (!obj) return GUIWIRE_STATUS_ERR_NO_SPACE;
    obj->x = 8u; obj->y = 24u; obj->w = 200u; obj->h = 10u; obj->fg = guiwire_rgb565(255,255,0); obj->bg = guiwire_rgb565(0,0,0); obj->flags = 0u; obj->zoom = 1u;
    strncpy(obj->text, "Waiting for client...", GUIWIRE_MAX_TEXT_LEN); obj->text[GUIWIRE_MAX_TEXT_LEN] = '\0';
    st = guiwire_object_text_draw(server, obj); if (st != GUIWIRE_STATUS_OK) return st;
    return guiwire_server_flush(server);
}

guiwire_status_t guiwire_server_reset_scene(guiwire_server_t *server) {
    guiwire_status_t st;
    if (!server) return GUIWIRE_STATUS_ERR_INVALID_ARGUMENT;
    guiwire_server_clear_all_objects(server);
    server->last_client_string[0] = '\0';
    st = guiwire_server_fill_bg(server, 0u, 0u,
                                server->cfg.display_size.width,
                                server->cfg.display_size.height,
                                guiwire_rgb565(0, 0, 0));
    if (st != GUIWIRE_STATUS_OK) return st;
    return guiwire_server_show_waiting(server);
}

guiwire_status_t guiwire_server_redraw(guiwire_server_t *server) { return guiwire_server_show_boot(server); }

guiwire_status_t guiwire_server_handle_packet(guiwire_server_t *server, const uint8_t *packet, size_t packet_len) {
    guiwire_packet_view_t view; guiwire_status_t st;
    if (!server || !packet) return GUIWIRE_STATUS_ERR_INVALID_ARGUMENT;
    st = guiwire_protocol_parse(packet, packet_len, &view); if (st != GUIWIRE_STATUS_OK) return st;
    switch ((guiwire_command_t)view.header.command) {
        case GUIWIRE_CMD_PING:
            return GUIWIRE_STATUS_OK;
        case GUIWIRE_CMD_HELLO_STRING: {
            size_t n = view.header.payload_len;
            if (n > GUIWIRE_MAX_TEXT_LEN) return GUIWIRE_STATUS_ERR_PROTOCOL;
            memcpy(server->last_client_string, view.payload, n);
            server->last_client_string[n] = '\0';
            return GUIWIRE_STATUS_OK;
        }
        case GUIWIRE_CMD_DESTROY_ALL_OBJECTS:
            guiwire_server_clear_all_objects(server);
            st = guiwire_server_fill_bg(server, 0u, 0u, server->cfg.display_size.width, server->cfg.display_size.height, guiwire_rgb565(0,0,0));
            if (st != GUIWIRE_STATUS_OK) return st;
            return guiwire_server_flush(server);
        case GUIWIRE_CMD_CREATE_TEXT:
            return guiwire_object_text_create(server, view.payload, view.header.payload_len);
        case GUIWIRE_CMD_UPDATE_TEXT:
            return guiwire_object_text_update(server, view.payload, view.header.payload_len);
        case GUIWIRE_CMD_CREATE_RECT:
            return guiwire_object_rect_create(server, view.payload, view.header.payload_len);
        case GUIWIRE_CMD_UPDATE_RECT:
            return guiwire_object_rect_update(server, view.payload, view.header.payload_len);
        case GUIWIRE_CMD_CREATE_LINE:
            return guiwire_object_line_create(server, view.payload, view.header.payload_len);
        case GUIWIRE_CMD_UPDATE_LINE:
            return guiwire_object_line_update(server, view.payload, view.header.payload_len);
        case GUIWIRE_CMD_CREATE_BAR:
            return guiwire_object_bar_create(server, view.payload, view.header.payload_len);
        case GUIWIRE_CMD_UPDATE_BAR:
            return guiwire_object_bar_update(server, view.payload, view.header.payload_len);
        case GUIWIRE_CMD_CREATE_RING:
            return guiwire_object_ring_create(server, view.payload, view.header.payload_len);
        case GUIWIRE_CMD_UPDATE_RING:
            return guiwire_object_ring_update(server, view.payload, view.header.payload_len);
        case GUIWIRE_CMD_CREATE_WAVEFORM:
            return guiwire_object_waveform_create(server, view.payload, view.header.payload_len);
        case GUIWIRE_CMD_UPDATE_WAVEFORM:
            return guiwire_object_waveform_update(server, view.payload, view.header.payload_len);
        case GUIWIRE_CMD_DESTROY_OBJECT: {
            guiwire_object_id_t object_id;
            if (view.header.payload_len != 2u) return GUIWIRE_STATUS_ERR_PROTOCOL;
            object_id = guiwire_read_u16le(view.payload);

            st = guiwire_object_text_destroy(server, object_id);
            if (st == GUIWIRE_STATUS_OK) return st;

            st = guiwire_object_rect_destroy(server, object_id);
            if (st == GUIWIRE_STATUS_OK) return st;

            st = guiwire_object_line_destroy(server, object_id);
            if (st == GUIWIRE_STATUS_OK) return st;

            st = guiwire_object_bar_destroy(server, object_id);
            if (st == GUIWIRE_STATUS_OK) return st;

            st = guiwire_object_ring_destroy(server, object_id);
            if (st == GUIWIRE_STATUS_OK) return st;

            st = guiwire_object_waveform_destroy(server, object_id);
            if (st == GUIWIRE_STATUS_OK) return st;

            return GUIWIRE_STATUS_ERR_NOT_FOUND;
        }
        default:
            return GUIWIRE_STATUS_ERR_NOT_SUPPORTED;
    }
}
