#ifndef GUI2C_OBJECT_LINE_H
#define GUI2C_OBJECT_LINE_H

#include "gui2c/gui2c.h"

#ifdef __cplusplus
extern "C" {
#endif

gui2c_status_t gui2c_object_line_create(gui2c_server_t *server, const uint8_t *payload, uint16_t payload_len);
gui2c_status_t gui2c_object_line_update(gui2c_server_t *server, const uint8_t *payload, uint16_t payload_len);
gui2c_status_t gui2c_object_line_destroy(gui2c_server_t *server, gui2c_object_id_t object_id);
gui2c_status_t gui2c_object_line_draw(gui2c_server_t *server, const gui2c_line_object_t *obj, gui2c_color_t color);

#ifdef __cplusplus
}
#endif

#endif
