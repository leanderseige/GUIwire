
#include <string.h>
#include "lib/server/gui2c_server_objects.h"

gui2c_status_t gui2c_server_fill_bg(gui2c_server_t *server, uint16_t x, uint16_t y, uint16_t w, uint16_t h, gui2c_color_t color) {
    if (!server || !server->cfg.renderer || !server->cfg.renderer->fill_rect) return GUI2C_STATUS_ERR_INVALID_ARGUMENT;
    if (w == 0u || h == 0u) return GUI2C_STATUS_OK;
    return server->cfg.renderer->fill_rect(server->cfg.renderer_ctx, x, y, w, h, color);
}

gui2c_status_t gui2c_server_flush(gui2c_server_t *server) {
    if (!server) return GUI2C_STATUS_ERR_INVALID_ARGUMENT;
    if (server->cfg.renderer && server->cfg.renderer->flush) return server->cfg.renderer->flush(server->cfg.renderer_ctx);
    return GUI2C_STATUS_OK;
}

gui2c_text_object_t *gui2c_server_find_text_object(gui2c_server_t *server, gui2c_object_id_t object_id) {
    size_t i;
    if (!server) return NULL;
    for (i = 0; i < GUI2C_MAX_OBJECTS; ++i) {
        if (server->text_objects[i].in_use && server->text_objects[i].object_id == object_id) return &server->text_objects[i];
    }
    return NULL;
}

gui2c_text_object_t *gui2c_server_alloc_text_object(gui2c_server_t *server, gui2c_object_id_t object_id) {
    size_t i;
    if (!server) return NULL;
    for (i = 0; i < GUI2C_MAX_OBJECTS; ++i) {
        if (!server->text_objects[i].in_use) {
            memset(&server->text_objects[i], 0, sizeof(server->text_objects[i]));
            server->text_objects[i].in_use = true;
            server->text_objects[i].object_id = object_id;
            return &server->text_objects[i];
        }
    }
    return NULL;
}

gui2c_rect_object_t *gui2c_server_find_rect_object(gui2c_server_t *server, gui2c_object_id_t object_id) {
    size_t i;
    if (!server) return NULL;
    for (i = 0; i < GUI2C_MAX_OBJECTS; ++i) {
        if (server->rect_objects[i].in_use && server->rect_objects[i].object_id == object_id) {
            return &server->rect_objects[i];
        }
    }
    return NULL;
}

gui2c_rect_object_t *gui2c_server_alloc_rect_object(gui2c_server_t *server, gui2c_object_id_t object_id) {
    size_t i;
    if (!server) return NULL;
    for (i = 0; i < GUI2C_MAX_OBJECTS; ++i) {
        if (!server->rect_objects[i].in_use) {
            memset(&server->rect_objects[i], 0, sizeof(server->rect_objects[i]));
            server->rect_objects[i].in_use = true;
            server->rect_objects[i].object_id = object_id;
            return &server->rect_objects[i];
        }
    }
    return NULL;
}

gui2c_line_object_t *gui2c_server_find_line_object(gui2c_server_t *server, gui2c_object_id_t object_id) {
    size_t i;
    if (!server) return NULL;
    for (i = 0; i < GUI2C_MAX_OBJECTS; ++i) {
        if (server->line_objects[i].in_use && server->line_objects[i].object_id == object_id) {
            return &server->line_objects[i];
        }
    }
    return NULL;
}

gui2c_line_object_t *gui2c_server_alloc_line_object(gui2c_server_t *server, gui2c_object_id_t object_id) {
    size_t i;
    if (!server) return NULL;
    for (i = 0; i < GUI2C_MAX_OBJECTS; ++i) {
        if (!server->line_objects[i].in_use) {
            memset(&server->line_objects[i], 0, sizeof(server->line_objects[i]));
            server->line_objects[i].in_use = true;
            server->line_objects[i].object_id = object_id;
            return &server->line_objects[i];
        }
    }
    return NULL;
}

gui2c_bar_object_t *gui2c_server_find_bar_object(gui2c_server_t *server, gui2c_object_id_t object_id) {
    size_t i;
    if (!server) return NULL;
    for (i = 0; i < GUI2C_MAX_OBJECTS; ++i) {
        if (server->bar_objects[i].in_use && server->bar_objects[i].object_id == object_id) {
            return &server->bar_objects[i];
        }
    }
    return NULL;
}

gui2c_bar_object_t *gui2c_server_alloc_bar_object(gui2c_server_t *server, gui2c_object_id_t object_id) {
    size_t i;
    if (!server) return NULL;
    for (i = 0; i < GUI2C_MAX_OBJECTS; ++i) {
        if (!server->bar_objects[i].in_use) {
            memset(&server->bar_objects[i], 0, sizeof(server->bar_objects[i]));
            server->bar_objects[i].in_use = true;
            server->bar_objects[i].object_id = object_id;
            return &server->bar_objects[i];
        }
    }
    return NULL;
}

gui2c_ring_object_t *gui2c_server_find_ring_object(gui2c_server_t *server, gui2c_object_id_t object_id) {
    size_t i;
    if (!server) return NULL;
    for (i = 0; i < GUI2C_MAX_OBJECTS; ++i) {
        if (server->ring_objects[i].in_use && server->ring_objects[i].object_id == object_id) {
            return &server->ring_objects[i];
        }
    }
    return NULL;
}

gui2c_ring_object_t *gui2c_server_alloc_ring_object(gui2c_server_t *server, gui2c_object_id_t object_id) {
    size_t i;
    if (!server) return NULL;
    for (i = 0; i < GUI2C_MAX_OBJECTS; ++i) {
        if (!server->ring_objects[i].in_use) {
            memset(&server->ring_objects[i], 0, sizeof(server->ring_objects[i]));
            server->ring_objects[i].in_use = true;
            server->ring_objects[i].object_id = object_id;
            return &server->ring_objects[i];
        }
    }
    return NULL;
}

gui2c_waveform_object_t *gui2c_server_find_waveform_object(gui2c_server_t *server, gui2c_object_id_t object_id) {
    size_t i;
    if (!server) return NULL;
    for (i = 0; i < GUI2C_SERVER_MAX_WAVEFORM_OBJECTS; ++i) {
        if (server->waveform_objects[i].in_use && server->waveform_objects[i].object_id == object_id) {
            return &server->waveform_objects[i];
        }
    }
    return NULL;
}

gui2c_waveform_object_t *gui2c_server_alloc_waveform_object(gui2c_server_t *server, gui2c_object_id_t object_id) {
    size_t i;
    if (!server) return NULL;
    for (i = 0; i < GUI2C_SERVER_MAX_WAVEFORM_OBJECTS; ++i) {
        if (!server->waveform_objects[i].in_use) {
            memset(&server->waveform_objects[i], 0, sizeof(server->waveform_objects[i]));
            server->waveform_objects[i].in_use = true;
            server->waveform_objects[i].object_id = object_id;
            return &server->waveform_objects[i];
        }
    }
    return NULL;
}

void gui2c_server_clear_all_objects(gui2c_server_t *server) {
    size_t i;
    if (!server) return;
    for (i = 0; i < GUI2C_MAX_OBJECTS; ++i) {
        memset(&server->text_objects[i], 0, sizeof(server->text_objects[i]));
        memset(&server->rect_objects[i], 0, sizeof(server->rect_objects[i]));
        memset(&server->line_objects[i], 0, sizeof(server->line_objects[i]));
        memset(&server->bar_objects[i], 0, sizeof(server->bar_objects[i]));
        memset(&server->ring_objects[i], 0, sizeof(server->ring_objects[i]));
    }
    for (i = 0; i < GUI2C_SERVER_MAX_WAVEFORM_OBJECTS; ++i) {
        memset(&server->waveform_objects[i], 0, sizeof(server->waveform_objects[i]));
    }
}
