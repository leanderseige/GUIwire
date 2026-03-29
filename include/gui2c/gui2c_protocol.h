
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

typedef uint16_t gui2c_object_id_t;

typedef enum {
    GUI2C_CMD_PING = 0x01,
    GUI2C_CMD_HELLO_STRING = 0x02,
    GUI2C_CMD_DESTROY_ALL_OBJECTS = 0x10,
    GUI2C_CMD_CREATE_TEXT = 0x11,
    GUI2C_CMD_UPDATE_TEXT = 0x12,
    GUI2C_CMD_DESTROY_OBJECT = 0x13,
    GUI2C_CMD_CREATE_RECT = 0x14,
    GUI2C_CMD_UPDATE_RECT = 0x15
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
    GUI2C_TEXT_FLAG_BG_TRANSPARENT = 0x03u
};

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

#ifdef __cplusplus
}
#endif

#endif
