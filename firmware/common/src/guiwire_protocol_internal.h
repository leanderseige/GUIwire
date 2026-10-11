/*
 * SPDX-License-Identifier: MIT
 * Authors: any:frequency, seige.digital GbR, Leander Seige (2026)
 */

#ifndef GUIWIRE_PROTOCOL_INTERNAL_H
#define GUIWIRE_PROTOCOL_INTERNAL_H

#include "guiwire/guiwire_protocol.h"
#include "guiwire/guiwire_status.h"

#ifdef __cplusplus
extern "C" {
#endif

guiwire_status_t guiwire_protocol_encode(guiwire_command_t command,
                                     const uint8_t *payload,
                                     uint16_t payload_len,
                                     uint8_t *out_packet,
                                     size_t out_capacity,
                                     size_t *out_packet_len);

guiwire_status_t guiwire_protocol_parse(const uint8_t *packet,
                                    size_t packet_len,
                                    guiwire_packet_view_t *out_view);

void guiwire_write_u16le(uint8_t *dst, uint16_t value);
uint16_t guiwire_read_u16le(const uint8_t *src);
void guiwire_write_u32le(uint8_t *dst, uint32_t value);
uint32_t guiwire_read_u32le(const uint8_t *src);

#ifdef __cplusplus
}
#endif

#endif
