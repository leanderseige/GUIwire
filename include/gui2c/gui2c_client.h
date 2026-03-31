#ifndef GUI2C_CLIENT_H
#define GUI2C_CLIENT_H

#include "gui2c/gui2c_protocol.h"
#include "gui2c/gui2c_status.h"
#include "gui2c/gui2c_transport.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    gui2c_transport_t transport;
} gui2c_client_t;

gui2c_status_t gui2c_client_init(gui2c_client_t *client, gui2c_transport_t transport);
gui2c_status_t gui2c_client_send_ping(gui2c_client_t *client);
gui2c_status_t gui2c_client_send_hello_string(gui2c_client_t *client, const char *text);
gui2c_status_t gui2c_client_destroy_all_objects(gui2c_client_t *client);
gui2c_status_t gui2c_client_create_text(gui2c_client_t *client, const gui2c_text_desc_t *desc);
gui2c_status_t gui2c_client_update_text(gui2c_client_t *client, const gui2c_text_update_t *update);
gui2c_status_t gui2c_client_destroy_object(gui2c_client_t *client, gui2c_object_id_t object_id);
gui2c_status_t gui2c_client_create_rect(gui2c_client_t *client, const gui2c_rect_desc_t *desc);
gui2c_status_t gui2c_client_update_rect(gui2c_client_t *client, const gui2c_rect_update_t *update);
gui2c_status_t gui2c_client_create_line(gui2c_client_t *client, const gui2c_line_desc_t *desc);
gui2c_status_t gui2c_client_update_line(gui2c_client_t *client, const gui2c_line_update_t *update);
gui2c_status_t gui2c_client_create_bar(gui2c_client_t *client, const gui2c_bar_desc_t *desc);
gui2c_status_t gui2c_client_update_bar(gui2c_client_t *client, const gui2c_bar_update_t *update);
gui2c_status_t gui2c_client_create_ring(gui2c_client_t *client, const gui2c_ring_desc_t *desc);
gui2c_status_t gui2c_client_update_ring(gui2c_client_t *client, const gui2c_ring_update_t *update);
gui2c_status_t gui2c_client_create_waveform(gui2c_client_t *client, const gui2c_waveform_desc_t *desc);
gui2c_status_t gui2c_client_update_waveform_full(gui2c_client_t *client, const gui2c_waveform_full_update_t *update);
gui2c_status_t gui2c_client_update_waveform_shift(gui2c_client_t *client, const gui2c_waveform_shift_update_t *update);
gui2c_status_t gui2c_client_update_waveform_shift_multi(gui2c_client_t *client, const gui2c_waveform_shift_multi_update_t *update);

#ifdef __cplusplus
}
#endif

#endif
