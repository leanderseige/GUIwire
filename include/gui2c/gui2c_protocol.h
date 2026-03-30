
#ifndef GUI2C_PROTOCOL_H
#define GUI2C_PROTOCOL_H

#include <stddef.h>
#include <stdint.h>
#include "gui2c/gui2c_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GUI2C_MAGIC_0 0x47u
#define GUI2C_MAGIC_1 0x32u
#define GUI2C_PROTOCOL_VERSION 0x01u

#define GUI2C_MAX_PACKET_SIZE 256u
#define GUI2C_MAX_TEXT_LEN 64u
#define GUI2C_MAX_OBJECTS 32u
#define GUI2C_MAX_WAVEFORM_WAVES 4u

typedef uint16_t gui2c_object_id_t;

typedef enum {
    GUI2C_CMD_PING = 0x01,
    GUI2C_CMD_HELLO_STRING = 0x02,
    GUI2C_CMD_DESTROY_ALL_OBJECTS = 0x10,
    GUI2C_CMD_CREATE_TEXT = 0x11,
    GUI2C_CMD_UPDATE_TEXT = 0x12,
    GUI2C_CMD_DESTROY_OBJECT = 0x13,
    GUI2C_CMD_CREATE_RECT = 0x14,
    GUI2C_CMD_UPDATE_RECT = 0x15,
    GUI2C_CMD_CREATE_LINE = 0x16,
    GUI2C_CMD_UPDATE_LINE = 0x17,
    GUI2C_CMD_CREATE_WAVEFORM = 0x18,
    GUI2C_CMD_UPDATE_WAVEFORM = 0x19,
    GUI2C_CMD_CREATE_BAR = 0x1A,
    GUI2C_CMD_UPDATE_BAR = 0x1B,
    GUI2C_CMD_CREATE_RING = 0x1C,
    GUI2C_CMD_UPDATE_RING = 0x1D
} gui2c_command_t;

typedef struct {
    uint8_t magic0;
    uint8_t magic1;
    uint8_t version;
    uint8_t command;
    uint16_t payload_len;
} gui2c_packet_header_t;

typedef struct {
    gui2c_packet_header_t header;
    const uint8_t *payload;
} gui2c_packet_view_t;

enum {
    GUI2C_RECT_FLAG_FILL_TRANSPARENT = 0x01u,
    GUI2C_RECT_FLAG_BORDER_ENABLED   = 0x02u,
    GUI2C_TEXT_FLAG_BG_TRANSPARENT = 0x03u,
    GUI2C_WAVEFORM_FLAG_BORDER_ENABLED = 0x01u
};

typedef enum {
    GUI2C_WAVEFORM_UPDATE_MODE_FULL_REPLACEMENT = 0x01u,
    GUI2C_WAVEFORM_UPDATE_MODE_SHIFTING = 0x02u,
    GUI2C_WAVEFORM_UPDATE_MODE_SHIFTING_MULTI = 0x03u
} gui2c_waveform_update_mode_t;

typedef enum {
    GUI2C_BAR_DIRECTION_TOP_TO_BOTTOM = 0x00u,
    GUI2C_BAR_DIRECTION_BOTTOM_TO_TOP = 0x01u,
    GUI2C_BAR_DIRECTION_LEFT_TO_RIGHT = 0x02u,
    GUI2C_BAR_DIRECTION_RIGHT_TO_LEFT = 0x03u
} gui2c_bar_direction_t;

typedef struct {
    gui2c_object_id_t object_id;
    uint16_t x;
    uint16_t y;
    uint16_t w;
    uint16_t h;
    gui2c_color_t fg;
    gui2c_color_t bg;
    uint8_t flags;
    uint8_t zoom;
    const char *text;
} gui2c_text_desc_t;

typedef struct {
    gui2c_object_id_t object_id;
    uint16_t x;
    uint16_t y;
    uint16_t w;
    uint16_t h;
    gui2c_color_t fill;
    gui2c_color_t border;
    uint8_t flags;
} gui2c_rect_desc_t;

typedef struct {
    gui2c_object_id_t object_id;
    uint16_t x1;
    uint16_t y1;
    uint16_t x2;
    uint16_t y2;
    gui2c_color_t fg;
    gui2c_color_t bg;
    uint8_t thickness;
} gui2c_line_desc_t;

typedef struct {
    gui2c_object_id_t object_id;
    uint16_t x;
    uint16_t y;
    uint16_t w;
    uint16_t h;
    gui2c_color_t fg;
    gui2c_color_t bg;
    uint16_t value;
    uint8_t direction;
} gui2c_bar_desc_t;

typedef struct {
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
} gui2c_ring_desc_t;

typedef struct {
    gui2c_object_id_t object_id;
    uint16_t x;
    uint16_t y;
    uint16_t w;
    uint16_t h;
    gui2c_color_t border;
    gui2c_color_t bg;
    uint8_t flags;
    uint8_t wave_count;
    const gui2c_color_t *wave_colors;
} gui2c_waveform_desc_t;

typedef struct {
    gui2c_object_id_t object_id;
    uint8_t wave_index;
    uint8_t sample_count;
    const uint16_t *samples;
} gui2c_waveform_full_update_t;

typedef struct {
    gui2c_object_id_t object_id;
    uint8_t wave_index;
    uint16_t x_advance;
    uint16_t sample;
} gui2c_waveform_shift_update_t;

typedef struct {
    gui2c_object_id_t object_id;
    uint8_t wave_count;
    uint16_t x_advance;
    const uint8_t *wave_indices;
    const uint16_t *samples;
} gui2c_waveform_shift_multi_update_t;

typedef struct {
    gui2c_object_id_t object_id;
    uint16_t value;
} gui2c_bar_update_t;

typedef struct {
    gui2c_object_id_t object_id;
    uint16_t value;
} gui2c_ring_update_t;

#ifdef __cplusplus
}
#endif

#endif
