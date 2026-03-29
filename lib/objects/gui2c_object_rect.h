#ifndef GUI2C_OBJECT_RECT_H
#define GUI2C_OBJECT_RECT_H

#include "gui2c/gui2c.h"

#ifdef __cplusplus
extern "C" {
#endif

gui2c_status_t gui2c_object_rect_create(gui2c_server_t *server, const uint8_t *payload, uint16_t payload_len);
gui2c_status_t gui2c_object_rect_update(gui2c_server_t *server, const uint8_t *payload, uint16_t payload_len);
gui2c_status_t gui2c_object_rect_destroy(gui2c_server_t *server, gui2c_object_id_t object_id);
gui2c_status_t gui2c_object_rect_draw(gui2c_server_t *server, const gui2c_rect_object_t *obj);
static int rect_geometry_equal(const gui2c_rect_object_t *a, const gui2c_rect_desc_t *b);

#ifdef __cplusplus
}
#endif

#endif