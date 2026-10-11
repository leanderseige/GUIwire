/*
 * SPDX-License-Identifier: MIT
 * Authors: any:frequency, seige.digital GbR, Leander Seige (2026)
 */


#ifndef GUIWIRE_SERVER_H
#define GUIWIRE_SERVER_H

#include <stdbool.h>
#include "guiwire/guiwire_protocol.h"
#include "guiwire/guiwire_renderer.h"
#include "guiwire/guiwire_status.h"
#include "guiwire/guiwire_server_target_profile.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    guiwire_size_t display_size;
    const guiwire_renderer_vtable_t *renderer;
    void *renderer_ctx;
    const char *server_id;
} guiwire_server_config_t;

typedef struct {
    bool in_use;
    guiwire_object_id_t object_id;
    uint16_t x;
    uint16_t y;
    uint16_t w;
    uint16_t h;
    guiwire_color_t fg;
    guiwire_color_t bg;
    uint8_t flags;
    uint8_t zoom;
    char text[GUIWIRE_MAX_TEXT_LEN + 1];
} guiwire_text_object_t;

typedef struct {
    bool in_use;
    guiwire_object_id_t object_id;
    uint16_t x;
    uint16_t y;
    uint16_t w;
    uint16_t h;
    guiwire_color_t fill;
    guiwire_color_t border;
    uint8_t flags;
} guiwire_rect_object_t;

typedef struct {
    bool in_use;
    guiwire_object_id_t object_id;
    uint16_t x1;
    uint16_t y1;
    uint16_t x2;
    uint16_t y2;
    guiwire_color_t fg;
    guiwire_color_t bg;
    uint8_t thickness;
} guiwire_line_object_t;

typedef struct {
    bool in_use;
    guiwire_object_id_t object_id;
    uint16_t x;
    uint16_t y;
    uint16_t w;
    uint16_t h;
    guiwire_color_t fg;
    guiwire_color_t bg;
    uint16_t value;
    uint8_t direction;
} guiwire_bar_object_t;

typedef struct {
    bool in_use;
    guiwire_object_id_t object_id;
    uint16_t cx;
    uint16_t cy;
    uint16_t radius;
    uint16_t inner_radius;
    uint16_t opening_angle_deg;
    guiwire_color_t bg;
    guiwire_color_t fg_active;
    guiwire_color_t fg_inactive;
    uint16_t value;
} guiwire_ring_object_t;

typedef struct {
    bool in_use;
    guiwire_object_id_t object_id;
    uint16_t x;
    uint16_t y;
    uint16_t w;
    uint16_t h;
    guiwire_color_t border;
    guiwire_color_t bg;
    uint8_t flags;
    uint8_t wave_count;
    guiwire_color_t wave_colors[GUIWIRE_MAX_WAVEFORM_WAVES];
    uint8_t wave_modes[GUIWIRE_MAX_WAVEFORM_WAVES];
    uint16_t sample_counts[GUIWIRE_MAX_WAVEFORM_WAVES];
    uint16_t sample_values[GUIWIRE_MAX_WAVEFORM_WAVES][GUIWIRE_SERVER_MAX_WAVEFORM_SAMPLES];
    uint16_t sample_x_offsets[GUIWIRE_MAX_WAVEFORM_WAVES][GUIWIRE_SERVER_MAX_WAVEFORM_SAMPLES];
} guiwire_waveform_object_t;

typedef struct {
    guiwire_server_config_t cfg;
    char last_client_string[GUIWIRE_MAX_TEXT_LEN + 1];
    guiwire_text_object_t text_objects[GUIWIRE_MAX_OBJECTS];
    guiwire_rect_object_t rect_objects[GUIWIRE_MAX_OBJECTS];
    guiwire_line_object_t line_objects[GUIWIRE_MAX_OBJECTS];
    guiwire_bar_object_t bar_objects[GUIWIRE_MAX_OBJECTS];
    guiwire_ring_object_t ring_objects[GUIWIRE_MAX_OBJECTS];
    guiwire_waveform_object_t waveform_objects[GUIWIRE_SERVER_MAX_WAVEFORM_OBJECTS];
} guiwire_server_t;

guiwire_status_t guiwire_server_init(guiwire_server_t *server, const guiwire_server_config_t *cfg);
guiwire_status_t guiwire_server_show_boot(guiwire_server_t *server);
guiwire_status_t guiwire_server_show_waiting(guiwire_server_t *server);
guiwire_status_t guiwire_server_reset_scene(guiwire_server_t *server);
guiwire_status_t guiwire_server_redraw(guiwire_server_t *server);
guiwire_status_t guiwire_server_handle_packet(guiwire_server_t *server, const uint8_t *packet, size_t packet_len);

#ifdef __cplusplus
}
#endif

#endif
