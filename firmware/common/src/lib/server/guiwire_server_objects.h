/*
 * SPDX-License-Identifier: MIT
 * Authors: any:frequency, seige.digital GbR, Leander Seige (2026)
 */


#ifndef GUIWIRE_SERVER_OBJECTS_H
#define GUIWIRE_SERVER_OBJECTS_H

#include "guiwire/guiwire_server.h"

#ifdef __cplusplus
extern "C" {
#endif

guiwire_status_t guiwire_server_fill_bg(guiwire_server_t *server, uint16_t x, uint16_t y, uint16_t w, uint16_t h, guiwire_color_t color);
guiwire_status_t guiwire_server_flush(guiwire_server_t *server);
guiwire_text_object_t *guiwire_server_find_text_object(guiwire_server_t *server, guiwire_object_id_t object_id);
guiwire_text_object_t *guiwire_server_alloc_text_object(guiwire_server_t *server, guiwire_object_id_t object_id);
guiwire_rect_object_t *guiwire_server_find_rect_object(guiwire_server_t *server, guiwire_object_id_t object_id);
guiwire_rect_object_t *guiwire_server_alloc_rect_object(guiwire_server_t *server, guiwire_object_id_t object_id);
guiwire_line_object_t *guiwire_server_find_line_object(guiwire_server_t *server, guiwire_object_id_t object_id);
guiwire_line_object_t *guiwire_server_alloc_line_object(guiwire_server_t *server, guiwire_object_id_t object_id);
guiwire_bar_object_t *guiwire_server_find_bar_object(guiwire_server_t *server, guiwire_object_id_t object_id);
guiwire_bar_object_t *guiwire_server_alloc_bar_object(guiwire_server_t *server, guiwire_object_id_t object_id);
guiwire_ring_object_t *guiwire_server_find_ring_object(guiwire_server_t *server, guiwire_object_id_t object_id);
guiwire_ring_object_t *guiwire_server_alloc_ring_object(guiwire_server_t *server, guiwire_object_id_t object_id);
guiwire_waveform_object_t *guiwire_server_find_waveform_object(guiwire_server_t *server, guiwire_object_id_t object_id);
guiwire_waveform_object_t *guiwire_server_alloc_waveform_object(guiwire_server_t *server, guiwire_object_id_t object_id);
void guiwire_server_clear_all_objects(guiwire_server_t *server);

#ifdef __cplusplus
}
#endif

#endif
