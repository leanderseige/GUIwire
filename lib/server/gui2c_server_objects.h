
#ifndef GUI2C_SERVER_OBJECTS_H
#define GUI2C_SERVER_OBJECTS_H

#include "gui2c/gui2c.h"

#ifdef __cplusplus
extern "C" {
#endif

gui2c_status_t gui2c_server_fill_bg(gui2c_server_t *server, uint16_t x, uint16_t y, uint16_t w, uint16_t h, gui2c_color_t color);
gui2c_status_t gui2c_server_flush(gui2c_server_t *server);
gui2c_text_object_t *gui2c_server_find_text_object(gui2c_server_t *server, gui2c_object_id_t object_id);
gui2c_text_object_t *gui2c_server_alloc_text_object(gui2c_server_t *server, gui2c_object_id_t object_id);
gui2c_rect_object_t *gui2c_server_find_rect_object(gui2c_server_t *server, gui2c_object_id_t object_id);
gui2c_rect_object_t *gui2c_server_alloc_rect_object(gui2c_server_t *server, gui2c_object_id_t object_id);
void gui2c_server_clear_all_objects(gui2c_server_t *server);

#ifdef __cplusplus
}
#endif

#endif
