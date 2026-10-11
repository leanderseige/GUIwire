/*
 * SPDX-License-Identifier: MIT
 * Authors: any:frequency, seige.digital GbR, Leander Seige (2026)
 */

#include <string.h>

#include "guiwire/guiwire_protocol.h"
#include "guiwire_protocol_internal.h"

void guiwire_write_u16le(uint8_t *dst, uint16_t value) {
    dst[0] = (uint8_t)(value & 0xFFu);
    dst[1] = (uint8_t)((value >> 8) & 0xFFu);
}

uint16_t guiwire_read_u16le(const uint8_t *src) {
    return (uint16_t)src[0] | ((uint16_t)src[1] << 8);
}

void guiwire_write_u32le(uint8_t *dst, uint32_t value) {
    dst[0] = (uint8_t)(value & 0xFFu);
    dst[1] = (uint8_t)((value >> 8) & 0xFFu);
    dst[2] = (uint8_t)((value >> 16) & 0xFFu);
    dst[3] = (uint8_t)((value >> 24) & 0xFFu);
}

uint32_t guiwire_read_u32le(const uint8_t *src) {
    return (uint32_t)src[0] |
           ((uint32_t)src[1] << 8) |
           ((uint32_t)src[2] << 16) |
           ((uint32_t)src[3] << 24);
}

guiwire_status_t guiwire_protocol_encode(guiwire_command_t command,
                                     const uint8_t *payload,
                                     uint16_t payload_len,
                                     uint8_t *out_packet,
                                     size_t out_capacity,
                                     size_t *out_packet_len) {
    const size_t header_len = sizeof(guiwire_packet_header_t);
    if (!out_packet || !out_packet_len) return GUIWIRE_STATUS_ERR_INVALID_ARGUMENT;
    if (payload_len != 0u && !payload) return GUIWIRE_STATUS_ERR_INVALID_ARGUMENT;
    if (header_len + payload_len > out_capacity) return GUIWIRE_STATUS_ERR_BUFFER_TOO_SMALL;

    out_packet[0] = GUIWIRE_MAGIC_0;
    out_packet[1] = GUIWIRE_MAGIC_1;
    out_packet[2] = GUIWIRE_PROTOCOL_VERSION;
    out_packet[3] = (uint8_t)command;
    guiwire_write_u16le(&out_packet[4], payload_len);

    if (payload_len && payload) {
        memcpy(&out_packet[header_len], payload, payload_len);
    }

    *out_packet_len = header_len + payload_len;
    return GUIWIRE_STATUS_OK;
}

guiwire_status_t guiwire_protocol_parse(const uint8_t *packet,
                                    size_t packet_len,
                                    guiwire_packet_view_t *out_view) {
    guiwire_packet_header_t h;
    if (!packet || !out_view) return GUIWIRE_STATUS_ERR_INVALID_ARGUMENT;
    if (packet_len < sizeof(guiwire_packet_header_t)) return GUIWIRE_STATUS_ERR_PROTOCOL;

    h.magic0 = packet[0];
    h.magic1 = packet[1];
    h.version = packet[2];
    h.command = packet[3];
    h.payload_len = guiwire_read_u16le(&packet[4]);

    if (h.magic0 != GUIWIRE_MAGIC_0 || h.magic1 != GUIWIRE_MAGIC_1) return GUIWIRE_STATUS_ERR_PROTOCOL;
    if (h.version != GUIWIRE_PROTOCOL_VERSION) return GUIWIRE_STATUS_ERR_PROTOCOL;
    if ((size_t)sizeof(guiwire_packet_header_t) + h.payload_len != packet_len) return GUIWIRE_STATUS_ERR_PROTOCOL;

    out_view->header = h;
    out_view->payload = &packet[sizeof(guiwire_packet_header_t)];
    return GUIWIRE_STATUS_OK;
}
