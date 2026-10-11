/*
 * SPDX-License-Identifier: MIT
 * Authors: any:frequency, seige.digital GbR, Leander Seige (2026)
 */


#include <string.h>
#include "lib/server/guiwire_server_objects.h"

guiwire_status_t guiwire_server_fill_bg(guiwire_server_t *server, uint16_t x, uint16_t y, uint16_t w, uint16_t h, guiwire_color_t color) {
    if (!server || !server->cfg.renderer || !server->cfg.renderer->fill_rect) return GUIWIRE_STATUS_ERR_INVALID_ARGUMENT;
    if (w == 0u || h == 0u) return GUIWIRE_STATUS_OK;
    return server->cfg.renderer->fill_rect(server->cfg.renderer_ctx, x, y, w, h, color);
}

guiwire_status_t guiwire_server_flush(guiwire_server_t *server) {
    if (!server) return GUIWIRE_STATUS_ERR_INVALID_ARGUMENT;
    if (server->cfg.renderer && server->cfg.renderer->flush) return server->cfg.renderer->flush(server->cfg.renderer_ctx);
    return GUIWIRE_STATUS_OK;
}

guiwire_text_object_t *guiwire_server_find_text_object(guiwire_server_t *server, guiwire_object_id_t object_id) {
    size_t i;
    if (!server) return NULL;
    for (i = 0; i < GUIWIRE_MAX_OBJECTS; ++i) {
        if (server->text_objects[i].in_use && server->text_objects[i].object_id == object_id) return &server->text_objects[i];
    }
    return NULL;
}

guiwire_text_object_t *guiwire_server_alloc_text_object(guiwire_server_t *server, guiwire_object_id_t object_id) {
    size_t i;
    if (!server) return NULL;
    for (i = 0; i < GUIWIRE_MAX_OBJECTS; ++i) {
        if (!server->text_objects[i].in_use) {
            memset(&server->text_objects[i], 0, sizeof(server->text_objects[i]));
            server->text_objects[i].in_use = true;
            server->text_objects[i].object_id = object_id;
            return &server->text_objects[i];
        }
    }
    return NULL;
}

guiwire_rect_object_t *guiwire_server_find_rect_object(guiwire_server_t *server, guiwire_object_id_t object_id) {
    size_t i;
    if (!server) return NULL;
    for (i = 0; i < GUIWIRE_MAX_OBJECTS; ++i) {
        if (server->rect_objects[i].in_use && server->rect_objects[i].object_id == object_id) {
            return &server->rect_objects[i];
        }
    }
    return NULL;
}

guiwire_rect_object_t *guiwire_server_alloc_rect_object(guiwire_server_t *server, guiwire_object_id_t object_id) {
    size_t i;
    if (!server) return NULL;
    for (i = 0; i < GUIWIRE_MAX_OBJECTS; ++i) {
        if (!server->rect_objects[i].in_use) {
            memset(&server->rect_objects[i], 0, sizeof(server->rect_objects[i]));
            server->rect_objects[i].in_use = true;
            server->rect_objects[i].object_id = object_id;
            return &server->rect_objects[i];
        }
    }
    return NULL;
}

guiwire_line_object_t *guiwire_server_find_line_object(guiwire_server_t *server, guiwire_object_id_t object_id) {
    size_t i;
    if (!server) return NULL;
    for (i = 0; i < GUIWIRE_MAX_OBJECTS; ++i) {
        if (server->line_objects[i].in_use && server->line_objects[i].object_id == object_id) {
            return &server->line_objects[i];
        }
    }
    return NULL;
}

guiwire_line_object_t *guiwire_server_alloc_line_object(guiwire_server_t *server, guiwire_object_id_t object_id) {
    size_t i;
    if (!server) return NULL;
    for (i = 0; i < GUIWIRE_MAX_OBJECTS; ++i) {
        if (!server->line_objects[i].in_use) {
            memset(&server->line_objects[i], 0, sizeof(server->line_objects[i]));
            server->line_objects[i].in_use = true;
            server->line_objects[i].object_id = object_id;
            return &server->line_objects[i];
        }
    }
    return NULL;
}

guiwire_bar_object_t *guiwire_server_find_bar_object(guiwire_server_t *server, guiwire_object_id_t object_id) {
    size_t i;
    if (!server) return NULL;
    for (i = 0; i < GUIWIRE_MAX_OBJECTS; ++i) {
        if (server->bar_objects[i].in_use && server->bar_objects[i].object_id == object_id) {
            return &server->bar_objects[i];
        }
    }
    return NULL;
}

guiwire_bar_object_t *guiwire_server_alloc_bar_object(guiwire_server_t *server, guiwire_object_id_t object_id) {
    size_t i;
    if (!server) return NULL;
    for (i = 0; i < GUIWIRE_MAX_OBJECTS; ++i) {
        if (!server->bar_objects[i].in_use) {
            memset(&server->bar_objects[i], 0, sizeof(server->bar_objects[i]));
            server->bar_objects[i].in_use = true;
            server->bar_objects[i].object_id = object_id;
            return &server->bar_objects[i];
        }
    }
    return NULL;
}

guiwire_ring_object_t *guiwire_server_find_ring_object(guiwire_server_t *server, guiwire_object_id_t object_id) {
    size_t i;
    if (!server) return NULL;
    for (i = 0; i < GUIWIRE_MAX_OBJECTS; ++i) {
        if (server->ring_objects[i].in_use && server->ring_objects[i].object_id == object_id) {
            return &server->ring_objects[i];
        }
    }
    return NULL;
}

guiwire_ring_object_t *guiwire_server_alloc_ring_object(guiwire_server_t *server, guiwire_object_id_t object_id) {
    size_t i;
    if (!server) return NULL;
    for (i = 0; i < GUIWIRE_MAX_OBJECTS; ++i) {
        if (!server->ring_objects[i].in_use) {
            memset(&server->ring_objects[i], 0, sizeof(server->ring_objects[i]));
            server->ring_objects[i].in_use = true;
            server->ring_objects[i].object_id = object_id;
            return &server->ring_objects[i];
        }
    }
    return NULL;
}

guiwire_waveform_object_t *guiwire_server_find_waveform_object(guiwire_server_t *server, guiwire_object_id_t object_id) {
    size_t i;
    if (!server) return NULL;
    for (i = 0; i < GUIWIRE_SERVER_MAX_WAVEFORM_OBJECTS; ++i) {
        if (server->waveform_objects[i].in_use && server->waveform_objects[i].object_id == object_id) {
            return &server->waveform_objects[i];
        }
    }
    return NULL;
}

guiwire_waveform_object_t *guiwire_server_alloc_waveform_object(guiwire_server_t *server, guiwire_object_id_t object_id) {
    size_t i;
    if (!server) return NULL;
    for (i = 0; i < GUIWIRE_SERVER_MAX_WAVEFORM_OBJECTS; ++i) {
        if (!server->waveform_objects[i].in_use) {
            memset(&server->waveform_objects[i], 0, sizeof(server->waveform_objects[i]));
            server->waveform_objects[i].in_use = true;
            server->waveform_objects[i].object_id = object_id;
            return &server->waveform_objects[i];
        }
    }
    return NULL;
}

void guiwire_server_clear_all_objects(guiwire_server_t *server) {
    size_t i;
    if (!server) return;
    for (i = 0; i < GUIWIRE_MAX_OBJECTS; ++i) {
        if (!server->text_objects[i].in_use || server->text_objects[i].object_id != 0u) {
            memset(&server->text_objects[i], 0, sizeof(server->text_objects[i]));
        }
        if (!server->rect_objects[i].in_use || server->rect_objects[i].object_id != 0u) {
            memset(&server->rect_objects[i], 0, sizeof(server->rect_objects[i]));
        }
        if (!server->line_objects[i].in_use || server->line_objects[i].object_id != 0u) {
            memset(&server->line_objects[i], 0, sizeof(server->line_objects[i]));
        }
        if (!server->bar_objects[i].in_use || server->bar_objects[i].object_id != 0u) {
            memset(&server->bar_objects[i], 0, sizeof(server->bar_objects[i]));
        }
        if (!server->ring_objects[i].in_use || server->ring_objects[i].object_id != 0u) {
            memset(&server->ring_objects[i], 0, sizeof(server->ring_objects[i]));
        }
    }
    for (i = 0; i < GUIWIRE_SERVER_MAX_WAVEFORM_OBJECTS; ++i) {
        if (!server->waveform_objects[i].in_use || server->waveform_objects[i].object_id != 0u) {
            memset(&server->waveform_objects[i], 0, sizeof(server->waveform_objects[i]));
        }
    }
}
