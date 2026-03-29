
#ifndef GUI2C_OBJECT_TEXT_H
#define GUI2C_OBJECT_TEXT_H
#include "gui2c/gui2c.h"
#ifdef __cplusplus
extern "C" {
#endif
gui2c_status_t gui2c_object_text_create(gui2c_server_t *server, const uint8_t *payload, uint16_t payload_len);
gui2c_status_t gui2c_object_text_update(gui2c_server_t *server, const uint8_t *payload, uint16_t payload_len);
gui2c_status_t gui2c_object_text_destroy(gui2c_server_t *server, gui2c_object_id_t object_id);
gui2c_status_t gui2c_object_text_draw(gui2c_server_t *server, const gui2c_text_object_t *obj);
#ifdef __cplusplus
}
#endif
#endif
