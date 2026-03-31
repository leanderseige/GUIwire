#include <string.h>

#include "gui2c/gui2c_client.h"
#include "lib/internal/gui2c_protocol_internal.h"

static gui2c_status_t gui2c_client_send_raw(gui2c_client_t *client,
                                            gui2c_command_t command,
                                            const uint8_t *payload,
                                            uint16_t payload_len) {
    uint8_t packet[GUI2C_MAX_PACKET_SIZE];
    size_t packet_len = 0;
    gui2c_status_t st;

    if (!client || !client->transport.write) return GUI2C_STATUS_ERR_INVALID_ARGUMENT;
    st = gui2c_protocol_encode(command, payload, payload_len, packet, sizeof(packet), &packet_len);
    if (st != GUI2C_STATUS_OK) return st;
    return client->transport.write(client->transport.ctx, packet, packet_len);
}

gui2c_status_t gui2c_client_init(gui2c_client_t *client, gui2c_transport_t transport) {
    if (!client || !transport.write) return GUI2C_STATUS_ERR_INVALID_ARGUMENT;
    client->transport = transport;
    return GUI2C_STATUS_OK;
}

gui2c_status_t gui2c_client_send_ping(gui2c_client_t *client) {
    return gui2c_client_send_raw(client, GUI2C_CMD_PING, NULL, 0);
}

gui2c_status_t gui2c_client_send_hello_string(gui2c_client_t *client, const char *text) {
    uint8_t payload[GUI2C_MAX_TEXT_LEN];
    size_t text_len;

    if (!client || !text) return GUI2C_STATUS_ERR_INVALID_ARGUMENT;
    text_len = strlen(text);
    if (text_len > GUI2C_MAX_TEXT_LEN) return GUI2C_STATUS_ERR_BUFFER_TOO_SMALL;

    memcpy(payload, text, text_len);
    return gui2c_client_send_raw(client, GUI2C_CMD_HELLO_STRING, payload, (uint16_t)text_len);
}

gui2c_status_t gui2c_client_destroy_all_objects(gui2c_client_t *client) {
    return gui2c_client_send_raw(client, GUI2C_CMD_DESTROY_ALL_OBJECTS, NULL, 0);
}

static gui2c_status_t gui2c_client_send_text_like(gui2c_client_t *client,
                                                  gui2c_command_t command,
                                                  const gui2c_text_desc_t *desc) {
    uint8_t payload[17 + GUI2C_MAX_TEXT_LEN];
    size_t text_len;
    uint16_t payload_len;

    if (!client || !desc || !desc->text) return GUI2C_STATUS_ERR_INVALID_ARGUMENT;

    text_len = strlen(desc->text);
    if (text_len > GUI2C_MAX_TEXT_LEN) return GUI2C_STATUS_ERR_BUFFER_TOO_SMALL;

    gui2c_write_u16le(&payload[0],  desc->object_id);
    gui2c_write_u16le(&payload[2],  desc->x);
    gui2c_write_u16le(&payload[4],  desc->y);
    gui2c_write_u16le(&payload[6],  desc->w);
    gui2c_write_u16le(&payload[8],  desc->h);
    gui2c_write_u16le(&payload[10], desc->fg);
    gui2c_write_u16le(&payload[12], desc->bg);
    payload[14] = desc->flags;
    payload[15] = desc->zoom;
    payload[16] = (uint8_t)text_len;
    memcpy(&payload[17], desc->text, text_len);

    payload_len = (uint16_t)(17 + text_len);
    return gui2c_client_send_raw(client, command, payload, payload_len);
}

gui2c_status_t gui2c_client_create_text(gui2c_client_t *client, const gui2c_text_desc_t *desc) {
    return gui2c_client_send_text_like(client, GUI2C_CMD_CREATE_TEXT, desc);
}

gui2c_status_t gui2c_client_update_text(gui2c_client_t *client, const gui2c_text_update_t *update) {
    uint8_t payload[11 + GUI2C_MAX_TEXT_LEN];
    size_t text_len;
    uint16_t payload_len;

    if (!client || !update || !update->text) return GUI2C_STATUS_ERR_INVALID_ARGUMENT;

    text_len = strlen(update->text);
    if (text_len > GUI2C_MAX_TEXT_LEN) return GUI2C_STATUS_ERR_BUFFER_TOO_SMALL;

    gui2c_write_u16le(&payload[0], update->object_id);
    gui2c_write_u16le(&payload[2], update->fg);
    gui2c_write_u16le(&payload[4], update->bg);
    payload[6] = update->flags;
    payload[7] = update->zoom;
    payload[8] = (uint8_t)text_len;
    memcpy(&payload[9], update->text, text_len);

    payload_len = (uint16_t)(9 + text_len);
    return gui2c_client_send_raw(client, GUI2C_CMD_UPDATE_TEXT, payload, payload_len);
}

gui2c_status_t gui2c_client_destroy_object(gui2c_client_t *client, gui2c_object_id_t object_id) {
    uint8_t payload[2];
    gui2c_write_u16le(payload, object_id);
    return gui2c_client_send_raw(client, GUI2C_CMD_DESTROY_OBJECT, payload, 2);
}

static gui2c_status_t gui2c_client_send_rect_like(gui2c_client_t *client,
                                                  gui2c_command_t command,
                                                  const gui2c_rect_desc_t *desc) {
    uint8_t payload[15];

    if (!client || !desc) return GUI2C_STATUS_ERR_INVALID_ARGUMENT;

    gui2c_write_u16le(&payload[0],  desc->object_id);
    gui2c_write_u16le(&payload[2],  desc->x);
    gui2c_write_u16le(&payload[4],  desc->y);
    gui2c_write_u16le(&payload[6],  desc->w);
    gui2c_write_u16le(&payload[8],  desc->h);
    gui2c_write_u16le(&payload[10], desc->fill);
    gui2c_write_u16le(&payload[12], desc->border);
    payload[14] = desc->flags;

    return gui2c_client_send_raw(client, command, payload, 15u);
}

gui2c_status_t gui2c_client_create_rect(gui2c_client_t *client, const gui2c_rect_desc_t *desc) {
    return gui2c_client_send_rect_like(client, GUI2C_CMD_CREATE_RECT, desc);
}

gui2c_status_t gui2c_client_update_rect(gui2c_client_t *client, const gui2c_rect_update_t *update) {
    uint8_t payload[7];

    if (!client || !update) return GUI2C_STATUS_ERR_INVALID_ARGUMENT;

    gui2c_write_u16le(&payload[0], update->object_id);
    gui2c_write_u16le(&payload[2], update->fill);
    gui2c_write_u16le(&payload[4], update->border);
    payload[6] = update->flags;

    return gui2c_client_send_raw(client, GUI2C_CMD_UPDATE_RECT, payload, 7u);
}

static gui2c_status_t gui2c_client_send_line_like(gui2c_client_t *client,
                                                  gui2c_command_t command,
                                                  const gui2c_line_desc_t *desc) {
    uint8_t payload[15];

    if (!client || !desc) return GUI2C_STATUS_ERR_INVALID_ARGUMENT;

    gui2c_write_u16le(&payload[0],  desc->object_id);
    gui2c_write_u16le(&payload[2],  desc->x1);
    gui2c_write_u16le(&payload[4],  desc->y1);
    gui2c_write_u16le(&payload[6],  desc->x2);
    gui2c_write_u16le(&payload[8],  desc->y2);
    gui2c_write_u16le(&payload[10], desc->fg);
    gui2c_write_u16le(&payload[12], desc->bg);
    payload[14] = desc->thickness;

    return gui2c_client_send_raw(client, command, payload, 15u);
}

gui2c_status_t gui2c_client_create_line(gui2c_client_t *client, const gui2c_line_desc_t *desc) {
    return gui2c_client_send_line_like(client, GUI2C_CMD_CREATE_LINE, desc);
}

gui2c_status_t gui2c_client_update_line(gui2c_client_t *client, const gui2c_line_update_t *update) {
    uint8_t payload[6];

    if (!client || !update) return GUI2C_STATUS_ERR_INVALID_ARGUMENT;

    gui2c_write_u16le(&payload[0], update->object_id);
    gui2c_write_u16le(&payload[2], update->fg);
    gui2c_write_u16le(&payload[4], update->bg);

    return gui2c_client_send_raw(client, GUI2C_CMD_UPDATE_LINE, payload, 6u);
}

static gui2c_status_t gui2c_client_send_bar_like(gui2c_client_t *client,
                                                 gui2c_command_t command,
                                                 const gui2c_bar_desc_t *desc) {
    uint8_t payload[17];

    if (!client || !desc) return GUI2C_STATUS_ERR_INVALID_ARGUMENT;

    gui2c_write_u16le(&payload[0],  desc->object_id);
    gui2c_write_u16le(&payload[2],  desc->x);
    gui2c_write_u16le(&payload[4],  desc->y);
    gui2c_write_u16le(&payload[6],  desc->w);
    gui2c_write_u16le(&payload[8],  desc->h);
    gui2c_write_u16le(&payload[10], desc->fg);
    gui2c_write_u16le(&payload[12], desc->bg);
    gui2c_write_u16le(&payload[14], desc->value);
    payload[16] = desc->direction;

    return gui2c_client_send_raw(client, command, payload, 17u);
}

gui2c_status_t gui2c_client_create_bar(gui2c_client_t *client, const gui2c_bar_desc_t *desc) {
    return gui2c_client_send_bar_like(client, GUI2C_CMD_CREATE_BAR, desc);
}

gui2c_status_t gui2c_client_update_bar(gui2c_client_t *client, const gui2c_bar_update_t *update) {
    uint8_t payload[4];

    if (!client || !update) return GUI2C_STATUS_ERR_INVALID_ARGUMENT;

    gui2c_write_u16le(&payload[0], update->object_id);
    gui2c_write_u16le(&payload[2], update->value);
    return gui2c_client_send_raw(client, GUI2C_CMD_UPDATE_BAR, payload, 4u);
}

gui2c_status_t gui2c_client_create_ring(gui2c_client_t *client, const gui2c_ring_desc_t *desc) {
    uint8_t payload[20];

    if (!client || !desc) return GUI2C_STATUS_ERR_INVALID_ARGUMENT;

    gui2c_write_u16le(&payload[0],  desc->object_id);
    gui2c_write_u16le(&payload[2],  desc->cx);
    gui2c_write_u16le(&payload[4],  desc->cy);
    gui2c_write_u16le(&payload[6],  desc->radius);
    gui2c_write_u16le(&payload[8],  desc->inner_radius);
    gui2c_write_u16le(&payload[10], desc->opening_angle_deg);
    gui2c_write_u16le(&payload[12], desc->bg);
    gui2c_write_u16le(&payload[14], desc->fg_active);
    gui2c_write_u16le(&payload[16], desc->fg_inactive);
    gui2c_write_u16le(&payload[18], desc->value);

    return gui2c_client_send_raw(client, GUI2C_CMD_CREATE_RING, payload, 20u);
}

gui2c_status_t gui2c_client_update_ring(gui2c_client_t *client, const gui2c_ring_update_t *update) {
    uint8_t payload[4];

    if (!client || !update) return GUI2C_STATUS_ERR_INVALID_ARGUMENT;

    gui2c_write_u16le(&payload[0], update->object_id);
    gui2c_write_u16le(&payload[2], update->value);
    return gui2c_client_send_raw(client, GUI2C_CMD_UPDATE_RING, payload, 4u);
}

gui2c_status_t gui2c_client_create_waveform(gui2c_client_t *client, const gui2c_waveform_desc_t *desc) {
    uint8_t payload[16u + (GUI2C_MAX_WAVEFORM_WAVES * 2u)];
    uint16_t payload_len;
    uint8_t i;

    if (!client || !desc || (desc->wave_count != 0u && !desc->wave_colors)) {
        return GUI2C_STATUS_ERR_INVALID_ARGUMENT;
    }
    if (desc->wave_count > GUI2C_MAX_WAVEFORM_WAVES) return GUI2C_STATUS_ERR_BUFFER_TOO_SMALL;

    gui2c_write_u16le(&payload[0],  desc->object_id);
    gui2c_write_u16le(&payload[2],  desc->x);
    gui2c_write_u16le(&payload[4],  desc->y);
    gui2c_write_u16le(&payload[6],  desc->w);
    gui2c_write_u16le(&payload[8],  desc->h);
    gui2c_write_u16le(&payload[10], desc->border);
    gui2c_write_u16le(&payload[12], desc->bg);
    payload[14] = desc->flags;
    payload[15] = desc->wave_count;
    for (i = 0; i < desc->wave_count; ++i) {
        gui2c_write_u16le(&payload[16u + (uint16_t)i * 2u], desc->wave_colors[i]);
    }

    payload_len = (uint16_t)(16u + (uint16_t)desc->wave_count * 2u);
    return gui2c_client_send_raw(client, GUI2C_CMD_CREATE_WAVEFORM, payload, payload_len);
}

gui2c_status_t gui2c_client_update_waveform_full(gui2c_client_t *client, const gui2c_waveform_full_update_t *update) {
    enum { GUI2C_MAX_WAVEFORM_FULL_UPDATE_SAMPLES = (GUI2C_MAX_PACKET_SIZE - 6u - 5u) / 2u };
    uint8_t payload[5u + (GUI2C_MAX_WAVEFORM_FULL_UPDATE_SAMPLES * 2u)];
    uint16_t payload_len;
    uint8_t i;

    if (!client || !update || (update->sample_count != 0u && !update->samples)) {
        return GUI2C_STATUS_ERR_INVALID_ARGUMENT;
    }
    if (update->sample_count > GUI2C_MAX_WAVEFORM_FULL_UPDATE_SAMPLES) return GUI2C_STATUS_ERR_BUFFER_TOO_SMALL;

    gui2c_write_u16le(&payload[0], update->object_id);
    payload[2] = (uint8_t)GUI2C_WAVEFORM_UPDATE_MODE_FULL_REPLACEMENT;
    payload[3] = update->wave_index;
    payload[4] = update->sample_count;
    for (i = 0; i < update->sample_count; ++i) {
        gui2c_write_u16le(&payload[5u + (uint16_t)i * 2u], update->samples[i]);
    }

    payload_len = (uint16_t)(5u + (uint16_t)update->sample_count * 2u);
    return gui2c_client_send_raw(client, GUI2C_CMD_UPDATE_WAVEFORM, payload, payload_len);
}

gui2c_status_t gui2c_client_update_waveform_shift(gui2c_client_t *client, const gui2c_waveform_shift_update_t *update) {
    uint8_t payload[8];

    if (!client || !update) return GUI2C_STATUS_ERR_INVALID_ARGUMENT;

    gui2c_write_u16le(&payload[0], update->object_id);
    payload[2] = (uint8_t)GUI2C_WAVEFORM_UPDATE_MODE_SHIFTING;
    payload[3] = update->wave_index;
    gui2c_write_u16le(&payload[4], update->x_advance);
    gui2c_write_u16le(&payload[6], update->sample);

    return gui2c_client_send_raw(client, GUI2C_CMD_UPDATE_WAVEFORM, payload, 8u);
}

gui2c_status_t gui2c_client_update_waveform_shift_multi(gui2c_client_t *client, const gui2c_waveform_shift_multi_update_t *update) {
    uint8_t payload[6u + (GUI2C_MAX_WAVEFORM_WAVES * 3u)];
    uint16_t payload_len;
    uint8_t i;

    if (!client || !update || update->wave_count == 0u || update->wave_count > GUI2C_MAX_WAVEFORM_WAVES ||
        !update->wave_indices || !update->samples) {
        return GUI2C_STATUS_ERR_INVALID_ARGUMENT;
    }

    gui2c_write_u16le(&payload[0], update->object_id);
    payload[2] = (uint8_t)GUI2C_WAVEFORM_UPDATE_MODE_SHIFTING_MULTI;
    payload[3] = update->wave_count;
    gui2c_write_u16le(&payload[4], update->x_advance);

    for (i = 0; i < update->wave_count; ++i) {
        uint16_t base = (uint16_t)(6u + (uint16_t)i * 3u);
        payload[base] = update->wave_indices[i];
        gui2c_write_u16le(&payload[base + 1u], update->samples[i]);
    }

    payload_len = (uint16_t)(6u + (uint16_t)update->wave_count * 3u);
    return gui2c_client_send_raw(client, GUI2C_CMD_UPDATE_WAVEFORM, payload, payload_len);
}
