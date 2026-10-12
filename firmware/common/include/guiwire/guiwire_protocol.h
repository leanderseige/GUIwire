/*
 * SPDX-License-Identifier: MIT
 * Authors: any:frequency, seige.digital GbR, Leander Seige (2026)
 */


#ifndef GUIWIRE_PROTOCOL_H
#define GUIWIRE_PROTOCOL_H

#include <stddef.h>
#include <stdint.h>
#include "guiwire/guiwire_types.h"
#include "guiwire/guiwire_status.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GUIWIRE_MAGIC_0 0x47u
#define GUIWIRE_MAGIC_1 0x32u
#define GUIWIRE_PROTOCOL_VERSION 0x01u
#define GUIWIRE_SESSION_TOKEN_SIZE 8u
#define GUIWIRE_SESSION_DATA_OVERHEAD 11u
#define GUIWIRE_SESSION_NO_ACK 0xFFFFu

#ifndef GUIWIRE_MAX_PACKET_SIZE
#define GUIWIRE_MAX_PACKET_SIZE 256u
#endif
#ifndef GUIWIRE_MAX_TEXT_LEN
#define GUIWIRE_MAX_TEXT_LEN 64u
#endif
#ifndef GUIWIRE_MAX_OBJECTS
#define GUIWIRE_MAX_OBJECTS 32u
#endif
#ifndef GUIWIRE_MAX_WAVEFORM_WAVES
#define GUIWIRE_MAX_WAVEFORM_WAVES 4u
#endif

typedef uint16_t guiwire_object_id_t;

typedef enum {
    GUIWIRE_CMD_PING = 0x01,
    GUIWIRE_CMD_HELLO_STRING = 0x02,
    GUIWIRE_CMD_POLL = 0x03,
    GUIWIRE_CMD_NO_DATA = 0x04,
    GUIWIRE_CMD_GET_LINK_INFO = 0x05,
    GUIWIRE_CMD_LINK_INFO = 0x06,
    GUIWIRE_CMD_SET_BAUD = 0x07,
    GUIWIRE_CMD_BAUD_ACK = 0x08,
    GUIWIRE_CMD_SESSION_DATA = 0x09,
    GUIWIRE_CMD_SESSION_EMPTY = 0x0A,
    GUIWIRE_CMD_SESSION_MISMATCH = 0x0B,
    GUIWIRE_CMD_DESTROY_ALL_OBJECTS = 0x10,
    GUIWIRE_CMD_CREATE_TEXT = 0x11,
    GUIWIRE_CMD_UPDATE_TEXT = 0x12,
    GUIWIRE_CMD_DESTROY_OBJECT = 0x13,
    GUIWIRE_CMD_CREATE_RECT = 0x14,
    GUIWIRE_CMD_UPDATE_RECT = 0x15,
    GUIWIRE_CMD_CREATE_LINE = 0x16,
    GUIWIRE_CMD_UPDATE_LINE = 0x17,
    GUIWIRE_CMD_CREATE_WAVEFORM = 0x18,
    GUIWIRE_CMD_UPDATE_WAVEFORM = 0x19,
    GUIWIRE_CMD_CREATE_BAR = 0x1A,
    GUIWIRE_CMD_UPDATE_BAR = 0x1B,
    GUIWIRE_CMD_CREATE_RING = 0x1C,
    GUIWIRE_CMD_UPDATE_RING = 0x1D
} guiwire_command_t;

typedef struct {
    uint8_t magic0;
    uint8_t magic1;
    uint8_t version;
    uint8_t command;
    uint16_t payload_len;
} guiwire_packet_header_t;

typedef struct {
    guiwire_packet_header_t header;
    const uint8_t *payload;
} guiwire_packet_view_t;

guiwire_status_t guiwire_protocol_encode(guiwire_command_t command,
                                         const uint8_t *payload,
                                         uint16_t payload_len,
                                         uint8_t *out_packet,
                                         size_t out_capacity,
                                         size_t *out_packet_len);

guiwire_status_t guiwire_protocol_parse(const uint8_t *packet,
                                        size_t packet_len,
                                        guiwire_packet_view_t *out_view);

enum {
    GUIWIRE_RECT_FLAG_FILL_TRANSPARENT = 0x01u,
    GUIWIRE_RECT_FLAG_BORDER_ENABLED   = 0x02u,
    GUIWIRE_TEXT_FLAG_BG_TRANSPARENT = 0x03u,
    GUIWIRE_WAVEFORM_FLAG_BORDER_ENABLED = 0x01u
};

typedef enum {
    GUIWIRE_WAVEFORM_UPDATE_MODE_FULL_REPLACEMENT = 0x01u,
    GUIWIRE_WAVEFORM_UPDATE_MODE_SHIFTING = 0x02u,
    GUIWIRE_WAVEFORM_UPDATE_MODE_SHIFTING_MULTI = 0x03u
} guiwire_waveform_update_mode_t;

typedef enum {
    GUIWIRE_BAR_DIRECTION_TOP_TO_BOTTOM = 0x00u,
    GUIWIRE_BAR_DIRECTION_BOTTOM_TO_TOP = 0x01u,
    GUIWIRE_BAR_DIRECTION_LEFT_TO_RIGHT = 0x02u,
    GUIWIRE_BAR_DIRECTION_RIGHT_TO_LEFT = 0x03u
} guiwire_bar_direction_t;

typedef struct {
    guiwire_object_id_t object_id;
    uint16_t x;
    uint16_t y;
    uint16_t w;
    uint16_t h;
    guiwire_color_t fg;
    guiwire_color_t bg;
    uint8_t flags;
    uint8_t zoom;
    const char *text;
} guiwire_text_desc_t;

typedef struct {
    guiwire_object_id_t object_id;
    guiwire_color_t fg;
    guiwire_color_t bg;
    uint8_t flags;
    uint8_t zoom;
    const char *text;
} guiwire_text_update_t;

typedef struct {
    guiwire_object_id_t object_id;
    uint16_t x;
    uint16_t y;
    uint16_t w;
    uint16_t h;
    guiwire_color_t fill;
    guiwire_color_t border;
    uint8_t flags;
} guiwire_rect_desc_t;

typedef struct {
    guiwire_object_id_t object_id;
    guiwire_color_t fill;
    guiwire_color_t border;
    uint8_t flags;
} guiwire_rect_update_t;

typedef struct {
    guiwire_object_id_t object_id;
    uint16_t x1;
    uint16_t y1;
    uint16_t x2;
    uint16_t y2;
    guiwire_color_t fg;
    guiwire_color_t bg;
    uint8_t thickness;
} guiwire_line_desc_t;

typedef struct {
    guiwire_object_id_t object_id;
    guiwire_color_t fg;
    guiwire_color_t bg;
} guiwire_line_update_t;

typedef struct {
    guiwire_object_id_t object_id;
    uint16_t x;
    uint16_t y;
    uint16_t w;
    uint16_t h;
    guiwire_color_t fg;
    guiwire_color_t bg;
    uint16_t value;
    uint8_t direction;
} guiwire_bar_desc_t;

typedef struct {
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
} guiwire_ring_desc_t;

typedef struct {
    guiwire_object_id_t object_id;
    uint16_t x;
    uint16_t y;
    uint16_t w;
    uint16_t h;
    guiwire_color_t border;
    guiwire_color_t bg;
    uint8_t flags;
    uint8_t wave_count;
    const guiwire_color_t *wave_colors;
} guiwire_waveform_desc_t;

typedef struct {
    guiwire_object_id_t object_id;
    uint8_t wave_index;
    uint8_t sample_count;
    const uint16_t *samples;
} guiwire_waveform_full_update_t;

typedef struct {
    guiwire_object_id_t object_id;
    uint8_t wave_index;
    uint16_t x_advance;
    uint16_t sample;
} guiwire_waveform_shift_update_t;

typedef struct {
    guiwire_object_id_t object_id;
    uint8_t wave_count;
    uint16_t x_advance;
    const uint8_t *wave_indices;
    const uint16_t *samples;
} guiwire_waveform_shift_multi_update_t;

typedef struct {
    guiwire_object_id_t object_id;
    uint16_t value;
} guiwire_bar_update_t;

typedef struct {
    guiwire_object_id_t object_id;
    uint16_t value;
} guiwire_ring_update_t;

#ifdef __cplusplus
}
#endif

#endif
