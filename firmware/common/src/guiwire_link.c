/*
 * SPDX-License-Identifier: MIT
 * Authors: any:frequency, seige.digital GbR, Leander Seige (2026)
 */

#include <string.h>

#include "guiwire/guiwire_link.h"
#include "guiwire/guiwire_protocol.h"
#include "guiwire/guiwire_status.h"
#include "guiwire_protocol_internal.h"

static uint16_t crc16_update(uint16_t crc, uint8_t byte) {
    uint8_t bit;

    crc ^= (uint16_t)byte << 8;
    for (bit = 0u; bit < 8u; ++bit) {
        crc = (crc & 0x8000u) ? (uint16_t)((crc << 1) ^ 0x1021u) : (uint16_t)(crc << 1);
    }
    return crc;
}

static guiwire_status_t write_byte(guiwire_link_t *link, uint8_t byte) {
    if (!link || !link->write) return GUIWIRE_STATUS_ERR_INVALID_ARGUMENT;
    return link->write(link->write_ctx, byte);
}

static guiwire_status_t write_body_byte(guiwire_link_t *link, uint8_t byte) {
    guiwire_status_t st;

    if (byte == GUIWIRE_SYNC_BYTE) {
        st = write_byte(link, GUIWIRE_ESCAPE_BYTE);
        if (st != GUIWIRE_STATUS_OK) return st;
        return write_byte(link, GUIWIRE_ESCAPE_SYNC);
    }
    if (byte == GUIWIRE_ESCAPE_BYTE) {
        st = write_byte(link, GUIWIRE_ESCAPE_BYTE);
        if (st != GUIWIRE_STATUS_OK) return st;
        return write_byte(link, GUIWIRE_ESCAPE_ESCAPE);
    }
    return write_byte(link, byte);
}

void guiwire_link_init(guiwire_link_t *link,
                       guiwire_link_write_fn write,
                       void *write_ctx,
                       guiwire_link_packet_fn on_packet,
                       void *packet_ctx) {
    if (!link) return;
    memset(link, 0, sizeof(*link));
    link->write = write;
    link->write_ctx = write_ctx;
    link->on_packet = on_packet;
    link->packet_ctx = packet_ctx;
    link->state = GUIWIRE_LINK_WAIT_SYNC;
}

void guiwire_link_reset(guiwire_link_t *link) {
    if (!link) return;
    link->state = GUIWIRE_LINK_WAIT_SYNC;
    link->escape_pending = false;
    link->frame_len = 0u;
    link->body_pos = 0u;
    link->crc = 0u;
    link->frame_invalid = false;
}

guiwire_status_t guiwire_link_send_packet(guiwire_link_t *link,
                                          const uint8_t *packet,
                                          size_t packet_len) {
    uint16_t frame_len;
    uint16_t crc = 0xFFFFu;
    uint16_t i;
    guiwire_status_t st;

    if (!link || !packet) return GUIWIRE_STATUS_ERR_INVALID_ARGUMENT;
    if (packet_len < sizeof(guiwire_packet_header_t) || packet_len > GUIWIRE_MAX_PACKET_SIZE) {
        return GUIWIRE_STATUS_ERR_BUFFER_TOO_SMALL;
    }
    st = guiwire_protocol_parse(packet, packet_len, &(guiwire_packet_view_t){0});
    if (st != GUIWIRE_STATUS_OK) return st;

    frame_len = (uint16_t)packet_len;
    st = write_byte(link, GUIWIRE_SYNC_BYTE);
    if (st != GUIWIRE_STATUS_OK) return st;

    st = write_body_byte(link, (uint8_t)(frame_len & 0xFFu));
    if (st != GUIWIRE_STATUS_OK) return st;
    crc = crc16_update(crc, (uint8_t)(frame_len & 0xFFu));
    st = write_body_byte(link, (uint8_t)(frame_len >> 8));
    if (st != GUIWIRE_STATUS_OK) return st;
    crc = crc16_update(crc, (uint8_t)(frame_len >> 8));

    for (i = 0u; i < frame_len; ++i) {
        st = write_body_byte(link, packet[i]);
        if (st != GUIWIRE_STATUS_OK) return st;
        crc = crc16_update(crc, packet[i]);
    }

    st = write_body_byte(link, (uint8_t)(crc & 0xFFu));
    if (st != GUIWIRE_STATUS_OK) return st;
    return write_body_byte(link, (uint8_t)(crc >> 8));
}

guiwire_status_t guiwire_link_send_command(guiwire_link_t *link,
                                           guiwire_command_t command,
                                           const uint8_t *payload,
                                           uint16_t payload_len) {
    uint8_t packet[GUIWIRE_MAX_PACKET_SIZE];
    size_t packet_len;
    guiwire_status_t st;

    st = guiwire_protocol_encode(command, payload, payload_len,
                                 packet, sizeof(packet), &packet_len);
    if (st != GUIWIRE_STATUS_OK) return st;
    return guiwire_link_send_packet(link, packet, packet_len);
}

static void start_frame(guiwire_link_t *link) {
    link->state = GUIWIRE_LINK_READ_BODY;
    link->escape_pending = false;
    link->frame_len = 0u;
    link->body_pos = 0u;
    link->crc = 0u;
    link->frame_invalid = false;
}

static void finish_frame(guiwire_link_t *link) {
    uint16_t received_crc;
    uint16_t crc = 0xFFFFu;
    uint16_t i;
    guiwire_packet_view_t view;

    if (link->frame_invalid || link->body_pos != (uint16_t)(2u + link->frame_len + 2u)) {
        link->invalid_frames++;
        guiwire_link_reset(link);
        return;
    }

    for (i = 0u; i < (uint16_t)(2u + link->frame_len); ++i) {
        crc = crc16_update(crc, link->body[i]);
    }
    received_crc = (uint16_t)link->body[2u + link->frame_len] |
                   ((uint16_t)link->body[3u + link->frame_len] << 8);

    if (crc != received_crc) {
        link->invalid_frames++;
        link->crc_errors++;
    } else if (guiwire_protocol_parse(&link->body[2], link->frame_len, &view) != GUIWIRE_STATUS_OK) {
        link->invalid_frames++;
        link->protocol_errors++;
    } else {
        link->valid_frames++;
        if (link->on_packet) link->on_packet(link->packet_ctx, &link->body[2], link->frame_len);
    }

    guiwire_link_reset(link);
}

static void feed_raw_body_byte(guiwire_link_t *link, uint8_t byte) {
    if (link->body_pos >= GUIWIRE_LINK_MAX_FRAME_BODY) {
        link->frame_invalid = true;
        return;
    }

    link->body[link->body_pos++] = byte;

    if (link->body_pos == 2u) {
        link->frame_len = (uint16_t)link->body[0] | ((uint16_t)link->body[1] << 8);
        if (link->frame_len < sizeof(guiwire_packet_header_t) ||
            link->frame_len > GUIWIRE_MAX_PACKET_SIZE) {
            link->frame_invalid = true;
        }
    }

    if (link->body_pos >= 2u && !link->frame_invalid) {
        uint16_t expected = (uint16_t)(2u + link->frame_len + 2u);
        if (link->body_pos == expected) finish_frame(link);
        else if (link->body_pos > expected) link->frame_invalid = true;
    }
}

void guiwire_link_feed_byte(guiwire_link_t *link, uint8_t byte) {
    if (!link) return;

    link->rx_bytes++;
    link->last_byte = byte;

    if (byte == GUIWIRE_SYNC_BYTE) {
        link->sync_bytes++;
        start_frame(link);
        return;
    }
    if (link->state == GUIWIRE_LINK_WAIT_SYNC) return;

    if (link->escape_pending) {
        link->escape_pending = false;
        if (byte == GUIWIRE_ESCAPE_SYNC) feed_raw_body_byte(link, GUIWIRE_SYNC_BYTE);
        else if (byte == GUIWIRE_ESCAPE_ESCAPE) feed_raw_body_byte(link, GUIWIRE_ESCAPE_BYTE);
        else link->frame_invalid = true;
        return;
    }

    if (byte == GUIWIRE_ESCAPE_BYTE) {
        link->escape_pending = true;
        return;
    }

    feed_raw_body_byte(link, byte);
}

void guiwire_link_feed(guiwire_link_t *link, const uint8_t *data, size_t len) {
    size_t i;
    if (!link || (!data && len != 0u)) return;
    for (i = 0u; i < len; ++i) guiwire_link_feed_byte(link, data[i]);
}
