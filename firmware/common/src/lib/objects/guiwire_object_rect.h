/*
 * SPDX-License-Identifier: MIT
 * Authors: any:frequency, seige.digital GbR, Leander Seige (2026)
 */

#ifndef GUIWIRE_OBJECT_RECT_H
#define GUIWIRE_OBJECT_RECT_H

#include "guiwire/guiwire_server.h"

#ifdef __cplusplus
extern "C" {
#endif

guiwire_status_t guiwire_object_rect_create(guiwire_server_t *server, const uint8_t *payload, uint16_t payload_len);
guiwire_status_t guiwire_object_rect_update(guiwire_server_t *server, const uint8_t *payload, uint16_t payload_len);
guiwire_status_t guiwire_object_rect_destroy(guiwire_server_t *server, guiwire_object_id_t object_id);
guiwire_status_t guiwire_object_rect_draw(guiwire_server_t *server, const guiwire_rect_object_t *obj);

#ifdef __cplusplus
}
#endif

#endif
