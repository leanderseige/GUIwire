/*
 * SPDX-License-Identifier: MIT
 * Authors: any:frequency, seige.digital GbR, Leander Seige (2026)
 */

#ifndef GUIWIRE_DEVICE_H
#define GUIWIRE_DEVICE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "guiwire/guiwire_protocol.h"
#include "guiwire/guiwire_status.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef GUIWIRE_DEVICE_QUEUE_CAPACITY
#define GUIWIRE_DEVICE_QUEUE_CAPACITY 4u
#endif

typedef struct {
    uint16_t len;
    uint8_t data[GUIWIRE_MAX_PACKET_SIZE];
} guiwire_device_queue_entry_t;

typedef struct {
    guiwire_device_queue_entry_t queue[GUIWIRE_DEVICE_QUEUE_CAPACITY];
    uint8_t head;
    uint8_t tail;
    uint8_t count;
    bool session_active;
} guiwire_device_t;

void guiwire_device_init(guiwire_device_t *device);
void guiwire_device_clear_queue(guiwire_device_t *device);
void guiwire_device_set_session(guiwire_device_t *device, bool active);
bool guiwire_device_session_active(const guiwire_device_t *device);

guiwire_status_t guiwire_device_queue_command(guiwire_device_t *device,
                                              guiwire_command_t command,
                                              const uint8_t *payload,
                                              uint16_t payload_len);

bool guiwire_device_pop(guiwire_device_t *device, uint8_t *packet, size_t capacity, size_t *out_len);
size_t guiwire_device_pending(const guiwire_device_t *device);

guiwire_status_t guiwire_device_queue_ping(guiwire_device_t *device);
guiwire_status_t guiwire_device_queue_hello(guiwire_device_t *device, const char *text);
guiwire_status_t guiwire_device_destroy_all_objects(guiwire_device_t *device);
guiwire_status_t guiwire_device_create_text(guiwire_device_t *device, const guiwire_text_desc_t *desc);
guiwire_status_t guiwire_device_update_text(guiwire_device_t *device, const guiwire_text_update_t *update);
guiwire_status_t guiwire_device_destroy_object(guiwire_device_t *device, guiwire_object_id_t object_id);
guiwire_status_t guiwire_device_create_rect(guiwire_device_t *device, const guiwire_rect_desc_t *desc);
guiwire_status_t guiwire_device_update_rect(guiwire_device_t *device, const guiwire_rect_update_t *update);
guiwire_status_t guiwire_device_create_line(guiwire_device_t *device, const guiwire_line_desc_t *desc);
guiwire_status_t guiwire_device_update_line(guiwire_device_t *device, const guiwire_line_update_t *update);
guiwire_status_t guiwire_device_create_bar(guiwire_device_t *device, const guiwire_bar_desc_t *desc);
guiwire_status_t guiwire_device_update_bar(guiwire_device_t *device, const guiwire_bar_update_t *update);
guiwire_status_t guiwire_device_create_ring(guiwire_device_t *device, const guiwire_ring_desc_t *desc);
guiwire_status_t guiwire_device_update_ring(guiwire_device_t *device, const guiwire_ring_update_t *update);
guiwire_status_t guiwire_device_create_waveform(guiwire_device_t *device, const guiwire_waveform_desc_t *desc);
guiwire_status_t guiwire_device_update_waveform_full(guiwire_device_t *device, const guiwire_waveform_full_update_t *update);
guiwire_status_t guiwire_device_update_waveform_shift(guiwire_device_t *device, const guiwire_waveform_shift_update_t *update);
guiwire_status_t guiwire_device_update_waveform_shift_multi(guiwire_device_t *device, const guiwire_waveform_shift_multi_update_t *update);

#ifdef __cplusplus
}
#endif

#endif
