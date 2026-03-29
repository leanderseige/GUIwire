#ifndef GUI2C_PROTOCOL_INTERNAL_H
#define GUI2C_PROTOCOL_INTERNAL_H

#include "gui2c/gui2c_protocol.h"
#include "gui2c/gui2c_status.h"

#ifdef __cplusplus
extern "C" {
#endif

gui2c_status_t gui2c_protocol_encode(gui2c_command_t command,
                                     const uint8_t *payload,
                                     uint16_t payload_len,
                                     uint8_t *out_packet,
                                     size_t out_capacity,
                                     size_t *out_packet_len);

gui2c_status_t gui2c_protocol_parse(const uint8_t *packet,
                                    size_t packet_len,
                                    gui2c_packet_view_t *out_view);

void gui2c_write_u16le(uint8_t *dst, uint16_t value);
uint16_t gui2c_read_u16le(const uint8_t *src);

#ifdef __cplusplus
}
#endif

#endif
