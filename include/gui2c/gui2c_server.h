
#ifndef GUI2C_SERVER_H
#define GUI2C_SERVER_H

#include <stdbool.h>
#include "gui2c/gui2c_protocol.h"
#include "gui2c/gui2c_renderer.h"
#include "gui2c/gui2c_status.h"
#include "gui2c/gui2c_server_target_profile.h"

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
    bool in_use;
    gui2c_object_id_t object_id;
    uint16_t x1;
    uint16_t y1;
    uint16_t x2;
    uint16_t y2;
    gui2c_color_t fg;
    gui2c_color_t bg;
    uint8_t thickness;
} gui2c_line_object_t;

typedef struct {
    bool in_use;
    gui2c_object_id_t object_id;
    uint16_t x;
    uint16_t y;
    uint16_t w;
    uint16_t h;
    gui2c_color_t fg;
    gui2c_color_t bg;
    uint16_t value;
    uint8_t direction;
} gui2c_bar_object_t;

typedef struct {
    bool in_use;
    gui2c_object_id_t object_id;
    uint16_t cx;
    uint16_t cy;
    uint16_t radius;
    uint16_t inner_radius;
    uint16_t opening_angle_deg;
    gui2c_color_t bg;
    gui2c_color_t fg_active;
    gui2c_color_t fg_inactive;
    uint16_t value;
} gui2c_ring_object_t;

typedef struct {
    bool in_use;
    gui2c_object_id_t object_id;
    uint16_t x;
    uint16_t y;
    uint16_t w;
    uint16_t h;
    gui2c_color_t border;
    gui2c_color_t bg;
    uint8_t flags;
    uint8_t wave_count;
    gui2c_color_t wave_colors[GUI2C_MAX_WAVEFORM_WAVES];
    uint8_t wave_modes[GUI2C_MAX_WAVEFORM_WAVES];
    uint16_t sample_counts[GUI2C_MAX_WAVEFORM_WAVES];
    uint16_t sample_values[GUI2C_MAX_WAVEFORM_WAVES][GUI2C_SERVER_MAX_WAVEFORM_SAMPLES];
    uint16_t sample_x_offsets[GUI2C_MAX_WAVEFORM_WAVES][GUI2C_SERVER_MAX_WAVEFORM_SAMPLES];
} gui2c_waveform_object_t;

typedef struct {
    gui2c_server_config_t cfg;
    char last_client_string[GUI2C_MAX_TEXT_LEN + 1];
    gui2c_text_object_t text_objects[GUI2C_MAX_OBJECTS];
    gui2c_rect_object_t rect_objects[GUI2C_MAX_OBJECTS];
    gui2c_line_object_t line_objects[GUI2C_MAX_OBJECTS];
    gui2c_bar_object_t bar_objects[GUI2C_MAX_OBJECTS];
    gui2c_ring_object_t ring_objects[GUI2C_MAX_OBJECTS];
    gui2c_waveform_object_t waveform_objects[GUI2C_SERVER_MAX_WAVEFORM_OBJECTS];
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
