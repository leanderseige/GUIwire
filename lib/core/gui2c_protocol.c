#include <string.h>

#include "gui2c/gui2c_protocol.h"
#include "lib/internal/gui2c_protocol_internal.h"

void gui2c_write_u16le(uint8_t *dst, uint16_t value) {
    dst[0] = (uint8_t)(value & 0xFFu);
    dst[1] = (uint8_t)((value >> 8) & 0xFFu);
}

uint16_t gui2c_read_u16le(const uint8_t *src) {
    return (uint16_t)src[0] | ((uint16_t)src[1] << 8);
}

gui2c_status_t gui2c_protocol_encode(gui2c_command_t command,
                                     const uint8_t *payload,
                                     uint16_t payload_len,
                                     uint8_t *out_packet,
                                     size_t out_capacity,
                                     size_t *out_packet_len) {
    const size_t header_len = sizeof(gui2c_packet_header_t);
    if (!out_packet || !out_packet_len) return GUI2C_STATUS_ERR_INVALID_ARGUMENT;
    if (header_len + payload_len > out_capacity) return GUI2C_STATUS_ERR_BUFFER_TOO_SMALL;

    out_packet[0] = GUI2C_MAGIC_0;
    out_packet[1] = GUI2C_MAGIC_1;
    out_packet[2] = GUI2C_PROTOCOL_VERSION;
    out_packet[3] = (uint8_t)command;
    gui2c_write_u16le(&out_packet[4], payload_len);

    if (payload_len && payload) {
        memcpy(&out_packet[header_len], payload, payload_len);
    }

    *out_packet_len = header_len + payload_len;
    return GUI2C_STATUS_OK;
}

gui2c_status_t gui2c_protocol_parse(const uint8_t *packet,
                                    size_t packet_len,
                                    gui2c_packet_view_t *out_view) {
    gui2c_packet_header_t h;
    if (!packet || !out_view) return GUI2C_STATUS_ERR_INVALID_ARGUMENT;
    if (packet_len < sizeof(gui2c_packet_header_t)) return GUI2C_STATUS_ERR_PROTOCOL;

    h.magic0 = packet[0];
    h.magic1 = packet[1];
    h.version = packet[2];
    h.command = packet[3];
    h.payload_len = gui2c_read_u16le(&packet[4]);

    if (h.magic0 != GUI2C_MAGIC_0 || h.magic1 != GUI2C_MAGIC_1) return GUI2C_STATUS_ERR_PROTOCOL;
    if (h.version != GUI2C_PROTOCOL_VERSION) return GUI2C_STATUS_ERR_PROTOCOL;
    if ((size_t)sizeof(gui2c_packet_header_t) + h.payload_len != packet_len) return GUI2C_STATUS_ERR_PROTOCOL;

    out_view->header = h;
    out_view->payload = &packet[sizeof(gui2c_packet_header_t)];
    return GUI2C_STATUS_OK;
}
