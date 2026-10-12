/*
 * SPDX-License-Identifier: MIT
 * Authors: any:frequency, seige.digital GbR, Leander Seige (2026)
 */

#include <string.h>

#include "guiwire/guiwire_device.h"
#include "guiwire_protocol_internal.h"

static guiwire_status_t guiwire_device_send_raw(guiwire_device_t *client,
                                                 guiwire_command_t command,
                                                 const uint8_t *payload,
                                                 uint16_t payload_len) {
    return guiwire_device_queue_command(client, command, payload, payload_len);
}

void guiwire_device_init(guiwire_device_t *device) {
    if (!device) return;
    memset(device, 0, sizeof(*device));
}

void guiwire_device_clear_queue(guiwire_device_t *device) {
    if (!device) return;
    device->head = 0u;
    device->tail = 0u;
    device->count = 0u;
}

void guiwire_device_set_session(guiwire_device_t *device, bool active) {
    if (!device) return;
    device->session_active = active;
    if (!active) guiwire_device_clear_queue(device);
}

bool guiwire_device_session_active(const guiwire_device_t *device) {
    return device && device->session_active;
}

guiwire_status_t guiwire_device_queue_command(guiwire_device_t *device,
                                               guiwire_command_t command,
                                               const uint8_t *payload,
                                               uint16_t payload_len) {
    size_t packet_len;
    guiwire_status_t st;

    if (!device) return GUIWIRE_STATUS_ERR_INVALID_ARGUMENT;
    if (device->count >= GUIWIRE_DEVICE_QUEUE_CAPACITY) return GUIWIRE_STATUS_ERR_NO_SPACE;

    st = guiwire_protocol_encode(command, payload, payload_len,
                                 device->queue[device->head].data,
                                 sizeof(device->queue[device->head].data),
                                 &packet_len);
    if (st != GUIWIRE_STATUS_OK) return st;
    /* A session DATA envelope needs eleven more bytes on the wire. Enforce
     * this at enqueue time so an unsendable packet cannot block the queue. */
    if (packet_len + GUIWIRE_SESSION_DATA_OVERHEAD > GUIWIRE_MAX_PACKET_SIZE) {
        return GUIWIRE_STATUS_ERR_BUFFER_TOO_SMALL;
    }

    device->queue[device->head].len = (uint16_t)packet_len;
    device->head = (uint8_t)((device->head + 1u) % GUIWIRE_DEVICE_QUEUE_CAPACITY);
    device->count++;
    return GUIWIRE_STATUS_OK;
}

bool guiwire_device_pop(guiwire_device_t *device, uint8_t *packet, size_t capacity, size_t *out_len) {
    const uint8_t *source;

    if (!device || !packet || !out_len || device->count == 0u) return false;
    if (!guiwire_device_peek(device, &source, out_len) || *out_len > capacity) return false;
    memcpy(packet, source, *out_len);
    return guiwire_device_commit(device);
}

bool guiwire_device_peek(const guiwire_device_t *device, const uint8_t **packet, size_t *out_len) {
    if (!device || !packet || !out_len || device->count == 0u) return false;
    *packet = device->queue[device->tail].data;
    *out_len = device->queue[device->tail].len;
    return true;
}

bool guiwire_device_commit(guiwire_device_t *device) {
    if (!device || device->count == 0u) return false;
    device->queue[device->tail].len = 0u;
    device->tail = (uint8_t)((device->tail + 1u) % GUIWIRE_DEVICE_QUEUE_CAPACITY);
    device->count--;
    return true;
}

size_t guiwire_device_pending(const guiwire_device_t *device) {
    return device ? device->count : 0u;
}

guiwire_status_t guiwire_device_queue_ping(guiwire_device_t *client) {
    return guiwire_device_send_raw(client, GUIWIRE_CMD_PING, NULL, 0);
}

guiwire_status_t guiwire_device_queue_hello(guiwire_device_t *client, const char *text) {
    uint8_t payload[GUIWIRE_MAX_TEXT_LEN];
    size_t text_len;

    if (!client || !text) return GUIWIRE_STATUS_ERR_INVALID_ARGUMENT;
    text_len = strlen(text);
    if (text_len > GUIWIRE_MAX_TEXT_LEN) return GUIWIRE_STATUS_ERR_BUFFER_TOO_SMALL;

    memcpy(payload, text, text_len);
    return guiwire_device_send_raw(client, GUIWIRE_CMD_HELLO_STRING, payload, (uint16_t)text_len);
}

guiwire_status_t guiwire_device_destroy_all_objects(guiwire_device_t *client) {
    return guiwire_device_send_raw(client, GUIWIRE_CMD_DESTROY_ALL_OBJECTS, NULL, 0);
}

static guiwire_status_t guiwire_device_send_text_like(guiwire_device_t *client,
                                                  guiwire_command_t command,
                                                  const guiwire_text_desc_t *desc) {
    uint8_t payload[17 + GUIWIRE_MAX_TEXT_LEN];
    size_t text_len;
    uint16_t payload_len;

    if (!client || !desc || !desc->text) return GUIWIRE_STATUS_ERR_INVALID_ARGUMENT;

    text_len = strlen(desc->text);
    if (text_len > GUIWIRE_MAX_TEXT_LEN) return GUIWIRE_STATUS_ERR_BUFFER_TOO_SMALL;

    guiwire_write_u16le(&payload[0],  desc->object_id);
    guiwire_write_u16le(&payload[2],  desc->x);
    guiwire_write_u16le(&payload[4],  desc->y);
    guiwire_write_u16le(&payload[6],  desc->w);
    guiwire_write_u16le(&payload[8],  desc->h);
    guiwire_write_u16le(&payload[10], desc->fg);
    guiwire_write_u16le(&payload[12], desc->bg);
    payload[14] = desc->flags;
    payload[15] = desc->zoom;
    payload[16] = (uint8_t)text_len;
    memcpy(&payload[17], desc->text, text_len);

    payload_len = (uint16_t)(17 + text_len);
    return guiwire_device_send_raw(client, command, payload, payload_len);
}

guiwire_status_t guiwire_device_create_text(guiwire_device_t *client, const guiwire_text_desc_t *desc) {
    return guiwire_device_send_text_like(client, GUIWIRE_CMD_CREATE_TEXT, desc);
}

guiwire_status_t guiwire_device_update_text(guiwire_device_t *client, const guiwire_text_update_t *update) {
    uint8_t payload[11 + GUIWIRE_MAX_TEXT_LEN];
    size_t text_len;
    uint16_t payload_len;

    if (!client || !update || !update->text) return GUIWIRE_STATUS_ERR_INVALID_ARGUMENT;

    text_len = strlen(update->text);
    if (text_len > GUIWIRE_MAX_TEXT_LEN) return GUIWIRE_STATUS_ERR_BUFFER_TOO_SMALL;

    guiwire_write_u16le(&payload[0], update->object_id);
    guiwire_write_u16le(&payload[2], update->fg);
    guiwire_write_u16le(&payload[4], update->bg);
    payload[6] = update->flags;
    payload[7] = update->zoom;
    payload[8] = (uint8_t)text_len;
    memcpy(&payload[9], update->text, text_len);

    payload_len = (uint16_t)(9 + text_len);
    return guiwire_device_send_raw(client, GUIWIRE_CMD_UPDATE_TEXT, payload, payload_len);
}

guiwire_status_t guiwire_device_destroy_object(guiwire_device_t *client, guiwire_object_id_t object_id) {
    uint8_t payload[2];
    guiwire_write_u16le(payload, object_id);
    return guiwire_device_send_raw(client, GUIWIRE_CMD_DESTROY_OBJECT, payload, 2);
}

static guiwire_status_t guiwire_device_send_rect_like(guiwire_device_t *client,
                                                  guiwire_command_t command,
                                                  const guiwire_rect_desc_t *desc) {
    uint8_t payload[15];

    if (!client || !desc) return GUIWIRE_STATUS_ERR_INVALID_ARGUMENT;

    guiwire_write_u16le(&payload[0],  desc->object_id);
    guiwire_write_u16le(&payload[2],  desc->x);
    guiwire_write_u16le(&payload[4],  desc->y);
    guiwire_write_u16le(&payload[6],  desc->w);
    guiwire_write_u16le(&payload[8],  desc->h);
    guiwire_write_u16le(&payload[10], desc->fill);
    guiwire_write_u16le(&payload[12], desc->border);
    payload[14] = desc->flags;

    return guiwire_device_send_raw(client, command, payload, 15u);
}

guiwire_status_t guiwire_device_create_rect(guiwire_device_t *client, const guiwire_rect_desc_t *desc) {
    return guiwire_device_send_rect_like(client, GUIWIRE_CMD_CREATE_RECT, desc);
}

guiwire_status_t guiwire_device_update_rect(guiwire_device_t *client, const guiwire_rect_update_t *update) {
    uint8_t payload[7];

    if (!client || !update) return GUIWIRE_STATUS_ERR_INVALID_ARGUMENT;

    guiwire_write_u16le(&payload[0], update->object_id);
    guiwire_write_u16le(&payload[2], update->fill);
    guiwire_write_u16le(&payload[4], update->border);
    payload[6] = update->flags;

    return guiwire_device_send_raw(client, GUIWIRE_CMD_UPDATE_RECT, payload, 7u);
}

static guiwire_status_t guiwire_device_send_line_like(guiwire_device_t *client,
                                                  guiwire_command_t command,
                                                  const guiwire_line_desc_t *desc) {
    uint8_t payload[15];

    if (!client || !desc) return GUIWIRE_STATUS_ERR_INVALID_ARGUMENT;

    guiwire_write_u16le(&payload[0],  desc->object_id);
    guiwire_write_u16le(&payload[2],  desc->x1);
    guiwire_write_u16le(&payload[4],  desc->y1);
    guiwire_write_u16le(&payload[6],  desc->x2);
    guiwire_write_u16le(&payload[8],  desc->y2);
    guiwire_write_u16le(&payload[10], desc->fg);
    guiwire_write_u16le(&payload[12], desc->bg);
    payload[14] = desc->thickness;

    return guiwire_device_send_raw(client, command, payload, 15u);
}

guiwire_status_t guiwire_device_create_line(guiwire_device_t *client, const guiwire_line_desc_t *desc) {
    return guiwire_device_send_line_like(client, GUIWIRE_CMD_CREATE_LINE, desc);
}

guiwire_status_t guiwire_device_update_line(guiwire_device_t *client, const guiwire_line_update_t *update) {
    uint8_t payload[6];

    if (!client || !update) return GUIWIRE_STATUS_ERR_INVALID_ARGUMENT;

    guiwire_write_u16le(&payload[0], update->object_id);
    guiwire_write_u16le(&payload[2], update->fg);
    guiwire_write_u16le(&payload[4], update->bg);

    return guiwire_device_send_raw(client, GUIWIRE_CMD_UPDATE_LINE, payload, 6u);
}

static guiwire_status_t guiwire_device_send_bar_like(guiwire_device_t *client,
                                                 guiwire_command_t command,
                                                 const guiwire_bar_desc_t *desc) {
    uint8_t payload[17];

    if (!client || !desc) return GUIWIRE_STATUS_ERR_INVALID_ARGUMENT;

    guiwire_write_u16le(&payload[0],  desc->object_id);
    guiwire_write_u16le(&payload[2],  desc->x);
    guiwire_write_u16le(&payload[4],  desc->y);
    guiwire_write_u16le(&payload[6],  desc->w);
    guiwire_write_u16le(&payload[8],  desc->h);
    guiwire_write_u16le(&payload[10], desc->fg);
    guiwire_write_u16le(&payload[12], desc->bg);
    guiwire_write_u16le(&payload[14], desc->value);
    payload[16] = desc->direction;

    return guiwire_device_send_raw(client, command, payload, 17u);
}

guiwire_status_t guiwire_device_create_bar(guiwire_device_t *client, const guiwire_bar_desc_t *desc) {
    return guiwire_device_send_bar_like(client, GUIWIRE_CMD_CREATE_BAR, desc);
}

guiwire_status_t guiwire_device_update_bar(guiwire_device_t *client, const guiwire_bar_update_t *update) {
    uint8_t payload[4];

    if (!client || !update) return GUIWIRE_STATUS_ERR_INVALID_ARGUMENT;

    guiwire_write_u16le(&payload[0], update->object_id);
    guiwire_write_u16le(&payload[2], update->value);
    return guiwire_device_send_raw(client, GUIWIRE_CMD_UPDATE_BAR, payload, 4u);
}

guiwire_status_t guiwire_device_create_ring(guiwire_device_t *client, const guiwire_ring_desc_t *desc) {
    uint8_t payload[20];

    if (!client || !desc) return GUIWIRE_STATUS_ERR_INVALID_ARGUMENT;

    guiwire_write_u16le(&payload[0],  desc->object_id);
    guiwire_write_u16le(&payload[2],  desc->cx);
    guiwire_write_u16le(&payload[4],  desc->cy);
    guiwire_write_u16le(&payload[6],  desc->radius);
    guiwire_write_u16le(&payload[8],  desc->inner_radius);
    guiwire_write_u16le(&payload[10], desc->opening_angle_deg);
    guiwire_write_u16le(&payload[12], desc->bg);
    guiwire_write_u16le(&payload[14], desc->fg_active);
    guiwire_write_u16le(&payload[16], desc->fg_inactive);
    guiwire_write_u16le(&payload[18], desc->value);

    return guiwire_device_send_raw(client, GUIWIRE_CMD_CREATE_RING, payload, 20u);
}

guiwire_status_t guiwire_device_update_ring(guiwire_device_t *client, const guiwire_ring_update_t *update) {
    uint8_t payload[4];

    if (!client || !update) return GUIWIRE_STATUS_ERR_INVALID_ARGUMENT;

    guiwire_write_u16le(&payload[0], update->object_id);
    guiwire_write_u16le(&payload[2], update->value);
    return guiwire_device_send_raw(client, GUIWIRE_CMD_UPDATE_RING, payload, 4u);
}

guiwire_status_t guiwire_device_create_waveform(guiwire_device_t *client, const guiwire_waveform_desc_t *desc) {
    uint8_t payload[16u + (GUIWIRE_MAX_WAVEFORM_WAVES * 2u)];
    uint16_t payload_len;
    uint8_t i;

    if (!client || !desc || (desc->wave_count != 0u && !desc->wave_colors)) {
        return GUIWIRE_STATUS_ERR_INVALID_ARGUMENT;
    }
    if (desc->wave_count > GUIWIRE_MAX_WAVEFORM_WAVES) return GUIWIRE_STATUS_ERR_BUFFER_TOO_SMALL;

    guiwire_write_u16le(&payload[0],  desc->object_id);
    guiwire_write_u16le(&payload[2],  desc->x);
    guiwire_write_u16le(&payload[4],  desc->y);
    guiwire_write_u16le(&payload[6],  desc->w);
    guiwire_write_u16le(&payload[8],  desc->h);
    guiwire_write_u16le(&payload[10], desc->border);
    guiwire_write_u16le(&payload[12], desc->bg);
    payload[14] = desc->flags;
    payload[15] = desc->wave_count;
    for (i = 0; i < desc->wave_count; ++i) {
        guiwire_write_u16le(&payload[16u + (uint16_t)i * 2u], desc->wave_colors[i]);
    }

    payload_len = (uint16_t)(16u + (uint16_t)desc->wave_count * 2u);
    return guiwire_device_send_raw(client, GUIWIRE_CMD_CREATE_WAVEFORM, payload, payload_len);
}

guiwire_status_t guiwire_device_update_waveform_full(guiwire_device_t *client, const guiwire_waveform_full_update_t *update) {
    enum { GUIWIRE_MAX_WAVEFORM_FULL_UPDATE_SAMPLES =
        (GUIWIRE_MAX_PACKET_SIZE - 6u - GUIWIRE_SESSION_DATA_OVERHEAD - 5u) / 2u };
    uint8_t payload[5u + (GUIWIRE_MAX_WAVEFORM_FULL_UPDATE_SAMPLES * 2u)];
    uint16_t payload_len;
    uint8_t i;

    if (!client || !update || (update->sample_count != 0u && !update->samples)) {
        return GUIWIRE_STATUS_ERR_INVALID_ARGUMENT;
    }
    if (update->sample_count > GUIWIRE_MAX_WAVEFORM_FULL_UPDATE_SAMPLES) return GUIWIRE_STATUS_ERR_BUFFER_TOO_SMALL;

    guiwire_write_u16le(&payload[0], update->object_id);
    payload[2] = (uint8_t)GUIWIRE_WAVEFORM_UPDATE_MODE_FULL_REPLACEMENT;
    payload[3] = update->wave_index;
    payload[4] = update->sample_count;
    for (i = 0; i < update->sample_count; ++i) {
        guiwire_write_u16le(&payload[5u + (uint16_t)i * 2u], update->samples[i]);
    }

    payload_len = (uint16_t)(5u + (uint16_t)update->sample_count * 2u);
    return guiwire_device_send_raw(client, GUIWIRE_CMD_UPDATE_WAVEFORM, payload, payload_len);
}

guiwire_status_t guiwire_device_update_waveform_shift(guiwire_device_t *client, const guiwire_waveform_shift_update_t *update) {
    uint8_t payload[8];

    if (!client || !update) return GUIWIRE_STATUS_ERR_INVALID_ARGUMENT;

    guiwire_write_u16le(&payload[0], update->object_id);
    payload[2] = (uint8_t)GUIWIRE_WAVEFORM_UPDATE_MODE_SHIFTING;
    payload[3] = update->wave_index;
    guiwire_write_u16le(&payload[4], update->x_advance);
    guiwire_write_u16le(&payload[6], update->sample);

    return guiwire_device_send_raw(client, GUIWIRE_CMD_UPDATE_WAVEFORM, payload, 8u);
}

guiwire_status_t guiwire_device_update_waveform_shift_multi(guiwire_device_t *client, const guiwire_waveform_shift_multi_update_t *update) {
    uint8_t payload[6u + (GUIWIRE_MAX_WAVEFORM_WAVES * 3u)];
    uint16_t payload_len;
    uint8_t i;

    if (!client || !update || update->wave_count == 0u || update->wave_count > GUIWIRE_MAX_WAVEFORM_WAVES ||
        !update->wave_indices || !update->samples) {
        return GUIWIRE_STATUS_ERR_INVALID_ARGUMENT;
    }

    guiwire_write_u16le(&payload[0], update->object_id);
    payload[2] = (uint8_t)GUIWIRE_WAVEFORM_UPDATE_MODE_SHIFTING_MULTI;
    payload[3] = update->wave_count;
    guiwire_write_u16le(&payload[4], update->x_advance);

    for (i = 0; i < update->wave_count; ++i) {
        uint16_t base = (uint16_t)(6u + (uint16_t)i * 3u);
        payload[base] = update->wave_indices[i];
        guiwire_write_u16le(&payload[base + 1u], update->samples[i]);
    }

    payload_len = (uint16_t)(6u + (uint16_t)update->wave_count * 3u);
    return guiwire_device_send_raw(client, GUIWIRE_CMD_UPDATE_WAVEFORM, payload, payload_len);
}
