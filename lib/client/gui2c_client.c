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

gui2c_status_t gui2c_client_update_text(gui2c_client_t *client, const gui2c_text_desc_t *desc) {
    return gui2c_client_send_text_like(client, GUI2C_CMD_UPDATE_TEXT, desc);
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

gui2c_status_t gui2c_client_update_rect(gui2c_client_t *client, const gui2c_rect_desc_t *desc) {
    return gui2c_client_send_rect_like(client, GUI2C_CMD_UPDATE_RECT, desc);
}