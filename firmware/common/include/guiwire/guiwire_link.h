/*
 * SPDX-License-Identifier: MIT
 * Authors: any:frequency, seige.digital GbR, Leander Seige (2026)
 */

#ifndef GUIWIRE_LINK_H
#define GUIWIRE_LINK_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "guiwire/guiwire_protocol.h"
#include "guiwire/guiwire_status.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GUIWIRE_SYNC_BYTE 0x7Eu
#define GUIWIRE_ESCAPE_BYTE 0x7Du
#define GUIWIRE_ESCAPE_SYNC 0x5Eu
#define GUIWIRE_ESCAPE_ESCAPE 0x5Du
#define GUIWIRE_LINK_MAX_FRAME_BODY (2u + GUIWIRE_MAX_PACKET_SIZE + 2u)

typedef guiwire_status_t (*guiwire_link_write_fn)(void *ctx, uint8_t byte);
typedef void (*guiwire_link_packet_fn)(void *ctx, const uint8_t *packet, size_t len);

typedef enum {
    GUIWIRE_LINK_WAIT_SYNC = 0,
    GUIWIRE_LINK_READ_BODY = 1
} guiwire_link_parser_state_t;

typedef struct {
    guiwire_link_write_fn write;
    void *write_ctx;
    guiwire_link_packet_fn on_packet;
    void *packet_ctx;
    guiwire_link_parser_state_t state;
    bool escape_pending;
    uint16_t frame_len;
    uint16_t body_pos;
    uint8_t body[GUIWIRE_LINK_MAX_FRAME_BODY];
    uint16_t crc;
    bool frame_invalid;
    uint32_t rx_bytes;
    uint32_t valid_frames;
    uint32_t invalid_frames;
    uint32_t crc_errors;
    uint32_t protocol_errors;
    uint32_t sync_bytes;
    uint8_t last_byte;
} guiwire_link_t;

void guiwire_link_init(guiwire_link_t *link,
                       guiwire_link_write_fn write,
                       void *write_ctx,
                       guiwire_link_packet_fn on_packet,
                       void *packet_ctx);

void guiwire_link_reset(guiwire_link_t *link);

guiwire_status_t guiwire_link_send_packet(guiwire_link_t *link,
                                          const uint8_t *packet,
                                          size_t packet_len);

guiwire_status_t guiwire_link_send_command(guiwire_link_t *link,
                                           guiwire_command_t command,
                                           const uint8_t *payload,
                                           uint16_t payload_len);

void guiwire_link_feed(guiwire_link_t *link, const uint8_t *data, size_t len);
void guiwire_link_feed_byte(guiwire_link_t *link, uint8_t byte);

#ifdef __cplusplus
}
#endif

#endif
