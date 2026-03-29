
#ifndef GUI2C_SERVER_H
#define GUI2C_SERVER_H

#include <stdbool.h>
#include "gui2c/gui2c_protocol.h"
#include "gui2c/gui2c_renderer.h"
#include "gui2c/gui2c_status.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    gui2c_size_t display_size;
    const gui2c_renderer_vtable_t *renderer;
    void *renderer_ctx;
    const char *server_id;
} gui2c_server_config_t;

typedef struct {
    bool in_use;
    gui2c_object_id_t object_id;
    uint16_t x;
    uint16_t y;
    uint16_t w;
    uint16_t h;
    gui2c_color_t fg;
    gui2c_color_t bg;
    uint8_t flags;
    uint8_t zoom;
    char text[GUI2C_MAX_TEXT_LEN + 1];
} gui2c_text_object_t;

typedef struct {
    bool in_use;
    gui2c_object_id_t object_id;
    uint16_t x;
    uint16_t y;
    uint16_t w;
    uint16_t h;
    gui2c_color_t fill;
    gui2c_color_t border;
    uint8_t flags;
} gui2c_rect_object_t;

typedef struct {
    gui2c_server_config_t cfg;
    char last_client_string[GUI2C_MAX_TEXT_LEN + 1];
    gui2c_text_object_t text_objects[GUI2C_MAX_OBJECTS];
    gui2c_rect_object_t rect_objects[GUI2C_MAX_OBJECTS];
} gui2c_server_t;

gui2c_status_t gui2c_server_init(gui2c_server_t *server, const gui2c_server_config_t *cfg);
gui2c_status_t gui2c_server_show_boot(gui2c_server_t *server);
gui2c_status_t gui2c_server_show_waiting(gui2c_server_t *server);
gui2c_status_t gui2c_server_redraw(gui2c_server_t *server);
gui2c_status_t gui2c_server_handle_packet(gui2c_server_t *server, const uint8_t *packet, size_t packet_len);

#ifdef __cplusplus
}
#endif

#endif
