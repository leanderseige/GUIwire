#include <string.h>

#include "lib/objects/gui2c_object_waveform.h"
#include "lib/internal/gui2c_protocol_internal.h"
#include "lib/server/gui2c_server_objects.h"

typedef struct {
    gui2c_object_id_t object_id;
    uint16_t x;
    uint16_t y;
    uint16_t w;
    uint16_t h;
    gui2c_color_t border;
    gui2c_color_t bg;
    uint8_t flags;
    uint8_t wave_count;
    gui2c_color_t wave_colors[GUI2C_MAX_WAVEFORM_WAVES];
} gui2c_waveform_create_desc_t;

typedef struct {
    gui2c_object_id_t object_id;
    uint8_t mode;
    uint8_t wave_index;
    uint8_t wave_count;
    uint8_t sample_count;
    uint16_t x_advance;
    uint16_t sample;
    const uint8_t *samples_payload;
    const uint8_t *multi_payload;
} gui2c_waveform_update_desc_t;

static gui2c_status_t parse_waveform_create_desc(const uint8_t *payload,
                                                 uint16_t payload_len,
                                                 gui2c_waveform_create_desc_t *out_desc) {
    uint8_t i;

    if (!payload || !out_desc) return GUI2C_STATUS_ERR_INVALID_ARGUMENT;
    if (payload_len < 16u) return GUI2C_STATUS_ERR_PROTOCOL;

    out_desc->object_id = gui2c_read_u16le(&payload[0]);
    out_desc->x = gui2c_read_u16le(&payload[2]);
    out_desc->y = gui2c_read_u16le(&payload[4]);
    out_desc->w = gui2c_read_u16le(&payload[6]);
    out_desc->h = gui2c_read_u16le(&payload[8]);
    out_desc->border = gui2c_read_u16le(&payload[10]);
    out_desc->bg = gui2c_read_u16le(&payload[12]);
    out_desc->flags = payload[14];
    out_desc->wave_count = payload[15];

    if (out_desc->wave_count == 0u || out_desc->wave_count > GUI2C_MAX_WAVEFORM_WAVES) {
        return GUI2C_STATUS_ERR_PROTOCOL;
    }
    if (payload_len != (uint16_t)(16u + (uint16_t)out_desc->wave_count * 2u)) {
        return GUI2C_STATUS_ERR_PROTOCOL;
    }

    for (i = 0; i < out_desc->wave_count; ++i) {
        out_desc->wave_colors[i] = gui2c_read_u16le(&payload[16u + (uint16_t)i * 2u]);
    }

    return GUI2C_STATUS_OK;
}

static gui2c_status_t parse_waveform_update_desc(const uint8_t *payload,
                                                 uint16_t payload_len,
                                                 gui2c_waveform_update_desc_t *out_desc) {
    if (!payload || !out_desc) return GUI2C_STATUS_ERR_INVALID_ARGUMENT;
    if (payload_len < 4u) return GUI2C_STATUS_ERR_PROTOCOL;

    out_desc->object_id = gui2c_read_u16le(&payload[0]);
    out_desc->mode = payload[2];
    out_desc->wave_index = payload[3];
    out_desc->wave_count = 0u;
    out_desc->sample_count = 0u;
    out_desc->x_advance = 0u;
    out_desc->sample = 0u;
    out_desc->samples_payload = NULL;
    out_desc->multi_payload = NULL;

    if (out_desc->mode == (uint8_t)GUI2C_WAVEFORM_UPDATE_MODE_FULL_REPLACEMENT) {
        if (payload_len < 5u) return GUI2C_STATUS_ERR_PROTOCOL;
        out_desc->sample_count = payload[4];
        if (out_desc->sample_count > GUI2C_SERVER_MAX_WAVEFORM_SAMPLES) return GUI2C_STATUS_ERR_PROTOCOL;
        if (payload_len != (uint16_t)(5u + (uint16_t)out_desc->sample_count * 2u)) {
            return GUI2C_STATUS_ERR_PROTOCOL;
        }
        out_desc->samples_payload = &payload[5];
        return GUI2C_STATUS_OK;
    }

    if (out_desc->mode == (uint8_t)GUI2C_WAVEFORM_UPDATE_MODE_SHIFTING) {
        if (payload_len != 8u) return GUI2C_STATUS_ERR_PROTOCOL;
        out_desc->x_advance = gui2c_read_u16le(&payload[4]);
        out_desc->sample = gui2c_read_u16le(&payload[6]);
        return GUI2C_STATUS_OK;
    }

    if (out_desc->mode == (uint8_t)GUI2C_WAVEFORM_UPDATE_MODE_SHIFTING_MULTI) {
        if (payload_len < 6u) return GUI2C_STATUS_ERR_PROTOCOL;
        out_desc->wave_count = payload[3];
        out_desc->x_advance = gui2c_read_u16le(&payload[4]);
        if (out_desc->wave_count == 0u || out_desc->wave_count > GUI2C_MAX_WAVEFORM_WAVES) {
            return GUI2C_STATUS_ERR_PROTOCOL;
        }
        if (payload_len != (uint16_t)(6u + (uint16_t)out_desc->wave_count * 3u)) {
            return GUI2C_STATUS_ERR_PROTOCOL;
        }
        out_desc->multi_payload = &payload[6];
        return GUI2C_STATUS_OK;
    }

    return GUI2C_STATUS_ERR_PROTOCOL;
}

static uint16_t waveform_plot_x(const gui2c_waveform_object_t *obj) {
    return (obj->flags & GUI2C_WAVEFORM_FLAG_BORDER_ENABLED) ? (uint16_t)(obj->x + 1u) : obj->x;
}

static uint16_t waveform_plot_y(const gui2c_waveform_object_t *obj) {
    return (obj->flags & GUI2C_WAVEFORM_FLAG_BORDER_ENABLED) ? (uint16_t)(obj->y + 1u) : obj->y;
}

static uint16_t waveform_plot_w(const gui2c_waveform_object_t *obj) {
    if ((obj->flags & GUI2C_WAVEFORM_FLAG_BORDER_ENABLED) == 0u) return obj->w;
    return (obj->w > 2u) ? (uint16_t)(obj->w - 2u) : 0u;
}

static uint16_t waveform_plot_h(const gui2c_waveform_object_t *obj) {
    if ((obj->flags & GUI2C_WAVEFORM_FLAG_BORDER_ENABLED) == 0u) return obj->h;
    return (obj->h > 2u) ? (uint16_t)(obj->h - 2u) : 0u;
}

static gui2c_status_t waveform_clear(gui2c_server_t *server, const gui2c_waveform_object_t *obj) {
    if (!server || !obj || !obj->in_use) return GUI2C_STATUS_ERR_INVALID_ARGUMENT;
    return gui2c_server_fill_bg(server, obj->x, obj->y, obj->w, obj->h, obj->bg);
}

static gui2c_status_t waveform_clear_plot(gui2c_server_t *server, const gui2c_waveform_object_t *obj) {
    uint16_t plot_x;
    uint16_t plot_y;
    uint16_t plot_w;
    uint16_t plot_h;

    if (!server || !obj || !obj->in_use) return GUI2C_STATUS_ERR_INVALID_ARGUMENT;

    plot_x = waveform_plot_x(obj);
    plot_y = waveform_plot_y(obj);
    plot_w = waveform_plot_w(obj);
    plot_h = waveform_plot_h(obj);
    if (plot_w == 0u || plot_h == 0u) return GUI2C_STATUS_OK;

    return gui2c_server_fill_bg(server, plot_x, plot_y, plot_w, plot_h, obj->bg);
}

static gui2c_status_t waveform_draw_frame(gui2c_server_t *server, const gui2c_waveform_object_t *obj) {
    gui2c_status_t st;

    if (!server || !obj || !obj->in_use) return GUI2C_STATUS_ERR_INVALID_ARGUMENT;
    if (obj->w == 0u || obj->h == 0u) return GUI2C_STATUS_OK;

    st = waveform_clear_plot(server, obj);
    if (st != GUI2C_STATUS_OK) return st;

    if ((obj->flags & GUI2C_WAVEFORM_FLAG_BORDER_ENABLED) == 0u) return GUI2C_STATUS_OK;

    if (obj->w == 1u || obj->h == 1u) {
        return gui2c_server_fill_bg(server, obj->x, obj->y, obj->w, obj->h, obj->border);
    }

    st = gui2c_server_fill_bg(server, obj->x, obj->y, obj->w, 1u, obj->border);
    if (st != GUI2C_STATUS_OK) return st;
    st = gui2c_server_fill_bg(server, obj->x, (uint16_t)(obj->y + obj->h - 1u), obj->w, 1u, obj->border);
    if (st != GUI2C_STATUS_OK) return st;
    st = gui2c_server_fill_bg(server, obj->x, obj->y, 1u, obj->h, obj->border);
    if (st != GUI2C_STATUS_OK) return st;
    return gui2c_server_fill_bg(server, (uint16_t)(obj->x + obj->w - 1u), obj->y, 1u, obj->h, obj->border);
}

static gui2c_status_t waveform_draw_point(gui2c_server_t *server,
                                          int x,
                                          int y,
                                          gui2c_color_t color,
                                          uint16_t clip_x,
                                          uint16_t clip_y,
                                          uint16_t clip_w,
                                          uint16_t clip_h) {
    if (!server || !server->cfg.renderer || !server->cfg.renderer->draw_pixel) {
        return GUI2C_STATUS_ERR_INVALID_ARGUMENT;
    }
    if (x < (int)clip_x || y < (int)clip_y) return GUI2C_STATUS_OK;
    if (x >= (int)(clip_x + clip_w) || y >= (int)(clip_y + clip_h)) return GUI2C_STATUS_OK;
    return server->cfg.renderer->draw_pixel(server->cfg.renderer_ctx, x, y, color);
}

static gui2c_status_t waveform_draw_vspan(gui2c_server_t *server,
                                          int x,
                                          int y0,
                                          int y1,
                                          gui2c_color_t color,
                                          uint16_t clip_x,
                                          uint16_t clip_y,
                                          uint16_t clip_w,
                                          uint16_t clip_h) {
    int top;
    int bottom;
    uint16_t draw_y;
    uint16_t draw_h;

    if (!server) return GUI2C_STATUS_ERR_INVALID_ARGUMENT;
    if (clip_w == 0u || clip_h == 0u) return GUI2C_STATUS_OK;
    if (x < (int)clip_x || x >= (int)(clip_x + clip_w)) return GUI2C_STATUS_OK;

    top = (y0 < y1) ? y0 : y1;
    bottom = (y0 > y1) ? y0 : y1;

    if (bottom < (int)clip_y || top >= (int)(clip_y + clip_h)) return GUI2C_STATUS_OK;
    if (top < (int)clip_y) top = (int)clip_y;
    if (bottom >= (int)(clip_y + clip_h)) bottom = (int)(clip_y + clip_h) - 1;
    if (bottom < top) return GUI2C_STATUS_OK;

    draw_y = (uint16_t)top;
    draw_h = (uint16_t)(bottom - top + 1);
    return gui2c_server_fill_bg(server, (uint16_t)x, draw_y, 1u, draw_h, color);
}

static gui2c_status_t waveform_draw_line(gui2c_server_t *server,
                                         int x0,
                                         int y0,
                                         int x1,
                                         int y1,
                                         gui2c_color_t color,
                                         uint16_t clip_x,
                                         uint16_t clip_y,
                                         uint16_t clip_w,
                                         uint16_t clip_h) {
    int left_x;
    int left_y;
    int right_x;
    int right_y;
    int dx;
    int x;

    if (!server) return GUI2C_STATUS_ERR_INVALID_ARGUMENT;

    if (x0 == x1) {
        return waveform_draw_vspan(server, x0, y0, y1, color, clip_x, clip_y, clip_w, clip_h);
    }

    if (x0 < x1) {
        left_x = x0;
        left_y = y0;
        right_x = x1;
        right_y = y1;
    } else {
        left_x = x1;
        left_y = y1;
        right_x = x0;
        right_y = y0;
    }

    dx = right_x - left_x;
    for (x = left_x; x <= right_x; ++x) {
        int rel0 = x - left_x;
        int rel1 = (x < right_x) ? (rel0 + 1) : rel0;
        int ya = left_y + ((right_y - left_y) * rel0) / dx;
        int yb = left_y + ((right_y - left_y) * rel1) / dx;
        gui2c_status_t st = waveform_draw_vspan(server, x, ya, yb, color, clip_x, clip_y, clip_w, clip_h);
        if (st != GUI2C_STATUS_OK) return st;
    }

    return GUI2C_STATUS_OK;
}

static uint16_t waveform_max_sample(const gui2c_waveform_object_t *obj) {
    uint8_t wave_idx;
    uint16_t sample_idx;
    uint16_t max_value = 0u;

    for (wave_idx = 0; wave_idx < obj->wave_count; ++wave_idx) {
        for (sample_idx = 0; sample_idx < obj->sample_counts[wave_idx]; ++sample_idx) {
            uint16_t value = obj->sample_values[wave_idx][sample_idx];
            if (value > max_value) max_value = value;
        }
    }

    return max_value;
}

static int waveform_sample_to_y(uint16_t value, uint16_t max_value, uint16_t plot_y, uint16_t plot_h) {
    uint32_t top_offset;

    if (plot_h == 0u) return (int)plot_y;
    if (max_value == 0u) return (int)(plot_y + plot_h - 1u);

    top_offset = ((uint32_t)value * (uint32_t)(plot_h - 1u)) / (uint32_t)max_value;
    return (int)(plot_y + (plot_h - 1u) - (uint16_t)top_offset);
}

static int waveform_sample_to_x(const gui2c_waveform_object_t *obj,
                                uint8_t wave_idx,
                                uint16_t sample_idx,
                                uint16_t plot_x,
                                uint16_t plot_w) {
    uint16_t count = obj->sample_counts[wave_idx];

    if (count <= 1u || plot_w == 0u) return (int)plot_x;

    if (obj->wave_modes[wave_idx] == (uint8_t)GUI2C_WAVEFORM_UPDATE_MODE_SHIFTING) {
        uint16_t offset = obj->sample_x_offsets[wave_idx][sample_idx];
        if (offset >= plot_w) return (int)plot_x;
        return (int)(plot_x + plot_w - 1u - offset);
    }

    return (int)(plot_x + (((uint32_t)sample_idx * (uint32_t)(plot_w - 1u)) / (uint32_t)(count - 1u)));
}

static gui2c_status_t waveform_draw_wave(gui2c_server_t *server,
                                         const gui2c_waveform_object_t *obj,
                                         uint8_t wave_idx,
                                         uint16_t plot_x,
                                         uint16_t plot_y,
                                         uint16_t plot_w,
                                         uint16_t plot_h,
                                         uint16_t max_value,
                                         gui2c_color_t color) {
    uint16_t count = obj->sample_counts[wave_idx];
    uint16_t sample_idx;

    if (count == 0u || plot_w == 0u || plot_h == 0u) return GUI2C_STATUS_OK;

    if (count == 1u) {
        int x = waveform_sample_to_x(obj, wave_idx, 0u, plot_x, plot_w);
        int y = waveform_sample_to_y(obj->sample_values[wave_idx][0], max_value, plot_y, plot_h);
        return waveform_draw_point(server, x, y, color, plot_x, plot_y, plot_w, plot_h);
    }

    for (sample_idx = 1u; sample_idx < count; ++sample_idx) {
        int x0 = waveform_sample_to_x(obj, wave_idx, (uint16_t)(sample_idx - 1u), plot_x, plot_w);
        int y0 = waveform_sample_to_y(obj->sample_values[wave_idx][sample_idx - 1u], max_value, plot_y, plot_h);
        int x1 = waveform_sample_to_x(obj, wave_idx, sample_idx, plot_x, plot_w);
        int y1 = waveform_sample_to_y(obj->sample_values[wave_idx][sample_idx], max_value, plot_y, plot_h);
        gui2c_status_t st = waveform_draw_line(server, x0, y0, x1, y1, color, plot_x, plot_y, plot_w, plot_h);
        if (st != GUI2C_STATUS_OK) return st;
    }

    return GUI2C_STATUS_OK;
}

static gui2c_status_t waveform_draw_segment_column(gui2c_server_t *server,
                                                   int target_x,
                                                   int x0,
                                                   int y0,
                                                   int x1,
                                                   int y1,
                                                   gui2c_color_t color,
                                                   uint16_t clip_x,
                                                   uint16_t clip_y,
                                                   uint16_t clip_w,
                                                   uint16_t clip_h) {
    int left_x;
    int left_y;
    int right_x;
    int right_y;
    int rel0;
    int rel1;
    int dx;
    int ya;
    int yb;

    if (!server) return GUI2C_STATUS_ERR_INVALID_ARGUMENT;

    if (x0 == x1) {
        if (target_x != x0) return GUI2C_STATUS_OK;
        return waveform_draw_vspan(server, target_x, y0, y1, color, clip_x, clip_y, clip_w, clip_h);
    }

    if (x0 < x1) {
        left_x = x0;
        left_y = y0;
        right_x = x1;
        right_y = y1;
    } else {
        left_x = x1;
        left_y = y1;
        right_x = x0;
        right_y = y0;
    }

    if (target_x < left_x || target_x > right_x) return GUI2C_STATUS_OK;

    dx = right_x - left_x;
    rel0 = target_x - left_x;
    rel1 = (target_x < right_x) ? (rel0 + 1) : rel0;
    ya = left_y + ((right_y - left_y) * rel0) / dx;
    yb = left_y + ((right_y - left_y) * rel1) / dx;

    return waveform_draw_vspan(server, target_x, ya, yb, color, clip_x, clip_y, clip_w, clip_h);
}

static gui2c_status_t waveform_draw_wave_column(gui2c_server_t *server,
                                                const gui2c_waveform_object_t *obj,
                                                uint8_t wave_idx,
                                                int draw_x,
                                                uint16_t plot_x,
                                                uint16_t plot_y,
                                                uint16_t plot_w,
                                                uint16_t plot_h,
                                                uint16_t max_value,
                                                gui2c_color_t color) {
    uint16_t count = obj->sample_counts[wave_idx];
    uint16_t sample_idx;

    if (count == 0u || plot_w == 0u || plot_h == 0u) return GUI2C_STATUS_OK;

    if (count == 1u) {
        int x = waveform_sample_to_x(obj, wave_idx, 0u, plot_x, plot_w);
        int y = waveform_sample_to_y(obj->sample_values[wave_idx][0], max_value, plot_y, plot_h);
        if (x != draw_x) return GUI2C_STATUS_OK;
        return waveform_draw_point(server, x, y, color, plot_x, plot_y, plot_w, plot_h);
    }

    for (sample_idx = 1u; sample_idx < count; ++sample_idx) {
        int x0 = waveform_sample_to_x(obj, wave_idx, (uint16_t)(sample_idx - 1u), plot_x, plot_w);
        int y0 = waveform_sample_to_y(obj->sample_values[wave_idx][sample_idx - 1u], max_value, plot_y, plot_h);
        int x1 = waveform_sample_to_x(obj, wave_idx, sample_idx, plot_x, plot_w);
        int y1 = waveform_sample_to_y(obj->sample_values[wave_idx][sample_idx], max_value, plot_y, plot_h);
        gui2c_status_t st = waveform_draw_segment_column(server, draw_x, x0, y0, x1, y1, color, plot_x, plot_y, plot_w, plot_h);
        if (st != GUI2C_STATUS_OK) return st;
    }

    return GUI2C_STATUS_OK;
}

static void waveform_column_cache_init(int16_t *tops, int16_t *bottoms, uint16_t plot_w) {
    uint16_t i;

    for (i = 0u; i < plot_w; ++i) {
        tops[i] = -1;
        bottoms[i] = -1;
    }
}

static void waveform_column_cache_mark(int16_t *tops,
                                       int16_t *bottoms,
                                       uint16_t plot_w,
                                       int x,
                                       int y0,
                                       int y1) {
    int top;
    int bottom;

    if (!tops || !bottoms) return;
    if (x < 0 || x >= (int)plot_w) return;

    top = (y0 < y1) ? y0 : y1;
    bottom = (y0 > y1) ? y0 : y1;

    if (tops[x] < 0 || top < tops[x]) tops[x] = (int16_t)top;
    if (bottoms[x] < 0 || bottom > bottoms[x]) bottoms[x] = (int16_t)bottom;
}

static void waveform_column_cache_add_segment(int16_t *tops,
                                              int16_t *bottoms,
                                              uint16_t plot_w,
                                              int rel_x0,
                                              int y0,
                                              int rel_x1,
                                              int y1) {
    int left_x;
    int left_y;
    int right_x;
    int right_y;
    int x;
    int dx;

    if (!tops || !bottoms || plot_w == 0u) return;

    if (rel_x0 == rel_x1) {
        waveform_column_cache_mark(tops, bottoms, plot_w, rel_x0, y0, y1);
        return;
    }

    if (rel_x0 < rel_x1) {
        left_x = rel_x0;
        left_y = y0;
        right_x = rel_x1;
        right_y = y1;
    } else {
        left_x = rel_x1;
        left_y = y1;
        right_x = rel_x0;
        right_y = y0;
    }

    dx = right_x - left_x;
    for (x = left_x; x <= right_x; ++x) {
        int rel0 = x - left_x;
        int rel1 = (x < right_x) ? (rel0 + 1) : rel0;
        int ya = left_y + ((right_y - left_y) * rel0) / dx;
        int yb = left_y + ((right_y - left_y) * rel1) / dx;
        waveform_column_cache_mark(tops, bottoms, plot_w, x, ya, yb);
    }
}

static void waveform_prepare_wave_columns(const gui2c_waveform_object_t *obj,
                                          uint8_t wave_idx,
                                          uint16_t plot_x,
                                          uint16_t plot_y,
                                          uint16_t plot_w,
                                          uint16_t plot_h,
                                          uint16_t max_value,
                                          int16_t *tops,
                                          int16_t *bottoms) {
    uint16_t count;
    uint16_t sample_idx;

    if (!obj || !tops || !bottoms) return;

    waveform_column_cache_init(tops, bottoms, plot_w);

    count = obj->sample_counts[wave_idx];
    if (count == 0u || plot_w == 0u || plot_h == 0u) return;

    if (count == 1u) {
        int x = waveform_sample_to_x(obj, wave_idx, 0u, plot_x, plot_w) - (int)plot_x;
        int y = waveform_sample_to_y(obj->sample_values[wave_idx][0], max_value, plot_y, plot_h);
        waveform_column_cache_mark(tops, bottoms, plot_w, x, y, y);
        return;
    }

    for (sample_idx = 1u; sample_idx < count; ++sample_idx) {
        int x0 = waveform_sample_to_x(obj, wave_idx, (uint16_t)(sample_idx - 1u), plot_x, plot_w) - (int)plot_x;
        int y0 = waveform_sample_to_y(obj->sample_values[wave_idx][sample_idx - 1u], max_value, plot_y, plot_h);
        int x1 = waveform_sample_to_x(obj, wave_idx, sample_idx, plot_x, plot_w) - (int)plot_x;
        int y1 = waveform_sample_to_y(obj->sample_values[wave_idx][sample_idx], max_value, plot_y, plot_h);
        waveform_column_cache_add_segment(tops, bottoms, plot_w, x0, y0, x1, y1);
    }
}

static gui2c_status_t waveform_apply_full_update(gui2c_waveform_object_t *obj,
                                                 const gui2c_waveform_update_desc_t *update) {
    uint16_t i;

    if (!obj || !update) return GUI2C_STATUS_ERR_INVALID_ARGUMENT;
    if (update->wave_index >= obj->wave_count) return GUI2C_STATUS_ERR_PROTOCOL;

    obj->wave_modes[update->wave_index] = (uint8_t)GUI2C_WAVEFORM_UPDATE_MODE_FULL_REPLACEMENT;
    obj->sample_counts[update->wave_index] = update->sample_count;

    for (i = 0; i < update->sample_count; ++i) {
        obj->sample_values[update->wave_index][i] = gui2c_read_u16le(&update->samples_payload[(uint16_t)i * 2u]);
        obj->sample_x_offsets[update->wave_index][i] = 0u;
    }

    return GUI2C_STATUS_OK;
}

static void waveform_shift_compact(gui2c_waveform_object_t *obj, uint8_t wave_idx, uint16_t plot_w) {
    uint16_t src;
    uint16_t dst = 0u;
    uint16_t count = obj->sample_counts[wave_idx];

    for (src = 0u; src < count; ++src) {
        if (obj->sample_x_offsets[wave_idx][src] < plot_w) {
            if (dst != src) {
                obj->sample_values[wave_idx][dst] = obj->sample_values[wave_idx][src];
                obj->sample_x_offsets[wave_idx][dst] = obj->sample_x_offsets[wave_idx][src];
            }
            ++dst;
        }
    }

    obj->sample_counts[wave_idx] = dst;
}

static gui2c_status_t waveform_apply_shift_update_one(gui2c_waveform_object_t *obj,
                                                      uint8_t wave_idx,
                                                      uint16_t x_advance,
                                                      uint16_t sample) {
    uint16_t plot_w;
    uint16_t count;
    uint16_t i;

    if (!obj) return GUI2C_STATUS_ERR_INVALID_ARGUMENT;
    if (wave_idx >= obj->wave_count) return GUI2C_STATUS_ERR_PROTOCOL;

    plot_w = waveform_plot_w(obj);
    count = obj->sample_counts[wave_idx];
    if (plot_w == 0u) {
        obj->sample_counts[wave_idx] = 0u;
        return GUI2C_STATUS_OK;
    }

    if (obj->wave_modes[wave_idx] != (uint8_t)GUI2C_WAVEFORM_UPDATE_MODE_SHIFTING) {
        obj->sample_counts[wave_idx] = 0u;
        count = 0u;
    }

    obj->wave_modes[wave_idx] = (uint8_t)GUI2C_WAVEFORM_UPDATE_MODE_SHIFTING;

    if (x_advance >= plot_w) {
        obj->sample_counts[wave_idx] = 0u;
        count = 0u;
    } else {
        for (i = 0u; i < count; ++i) {
            uint32_t advanced = (uint32_t)obj->sample_x_offsets[wave_idx][i] + (uint32_t)x_advance;
            obj->sample_x_offsets[wave_idx][i] = (advanced > 0xFFFFu) ? 0xFFFFu : (uint16_t)advanced;
        }

        waveform_shift_compact(obj, wave_idx, plot_w);
        count = obj->sample_counts[wave_idx];
    }

    if (count == GUI2C_SERVER_MAX_WAVEFORM_SAMPLES) {
        memmove(&obj->sample_values[wave_idx][0], &obj->sample_values[wave_idx][1], (GUI2C_SERVER_MAX_WAVEFORM_SAMPLES - 1u) * sizeof(uint16_t));
        memmove(&obj->sample_x_offsets[wave_idx][0], &obj->sample_x_offsets[wave_idx][1], (GUI2C_SERVER_MAX_WAVEFORM_SAMPLES - 1u) * sizeof(uint16_t));
        count = (uint16_t)(GUI2C_SERVER_MAX_WAVEFORM_SAMPLES - 1u);
    }

    obj->sample_values[wave_idx][count] = sample;
    obj->sample_x_offsets[wave_idx][count] = 0u;
    obj->sample_counts[wave_idx] = (uint16_t)(count + 1u);

    waveform_shift_compact(obj, wave_idx, plot_w);
    return GUI2C_STATUS_OK;
}

static gui2c_status_t waveform_apply_shift_update(gui2c_waveform_object_t *obj,
                                                  const gui2c_waveform_update_desc_t *update) {
    if (!obj || !update) return GUI2C_STATUS_ERR_INVALID_ARGUMENT;
    return waveform_apply_shift_update_one(obj, update->wave_index, update->x_advance, update->sample);
}

static gui2c_status_t waveform_apply_shift_multi_update(gui2c_waveform_object_t *obj,
                                                        const gui2c_waveform_update_desc_t *update) {
    uint8_t i;

    if (!obj || !update || !update->multi_payload) return GUI2C_STATUS_ERR_INVALID_ARGUMENT;

    for (i = 0u; i < update->wave_count; ++i) {
        const uint8_t *entry = &update->multi_payload[(uint16_t)i * 3u];
        uint8_t wave_idx = entry[0];
        uint16_t sample = gui2c_read_u16le(&entry[1]);
        gui2c_status_t st = waveform_apply_shift_update_one(obj, wave_idx, update->x_advance, sample);
        if (st != GUI2C_STATUS_OK) return st;
    }

    return GUI2C_STATUS_OK;
}

static gui2c_status_t create_or_replace_waveform(gui2c_server_t *server,
                                                 const gui2c_waveform_create_desc_t *desc,
                                                 int replace_existing) {
    gui2c_waveform_object_t *obj;
    uint8_t i;

    if (!server || !desc) return GUI2C_STATUS_ERR_INVALID_ARGUMENT;

    obj = gui2c_server_find_waveform_object(server, desc->object_id);
    if (obj && !replace_existing) return GUI2C_STATUS_ERR_ALREADY_EXISTS;
    if (!obj) obj = gui2c_server_alloc_waveform_object(server, desc->object_id);
    if (!obj) return GUI2C_STATUS_ERR_NO_SPACE;

    obj->x = desc->x;
    obj->y = desc->y;
    obj->w = desc->w;
    obj->h = desc->h;
    obj->border = desc->border;
    obj->bg = desc->bg;
    obj->flags = desc->flags;
    obj->wave_count = desc->wave_count;

    for (i = 0u; i < GUI2C_MAX_WAVEFORM_WAVES; ++i) {
        obj->wave_colors[i] = (i < desc->wave_count) ? desc->wave_colors[i] : 0u;
        obj->wave_modes[i] = 0u;
        obj->sample_counts[i] = 0u;
    }

    return GUI2C_STATUS_OK;
}

static gui2c_status_t waveform_draw_all_waves(gui2c_server_t *server,
                                              const gui2c_waveform_object_t *obj,
                                              uint16_t plot_x,
                                              uint16_t plot_y,
                                              uint16_t plot_w,
                                              uint16_t plot_h,
                                              uint16_t max_value,
                                              int erase) {
    uint8_t wave_idx;

    for (wave_idx = 0u; wave_idx < obj->wave_count; ++wave_idx) {
        gui2c_color_t color = erase ? obj->bg : obj->wave_colors[wave_idx];
        gui2c_status_t st = waveform_draw_wave(server, obj, wave_idx, plot_x, plot_y, plot_w, plot_h, max_value, color);
        if (st != GUI2C_STATUS_OK) return st;
    }

    return GUI2C_STATUS_OK;
}

static gui2c_status_t waveform_render_plot_columns(gui2c_server_t *server, const gui2c_waveform_object_t *obj) {
    uint16_t plot_x;
    uint16_t plot_y;
    uint16_t plot_w;
    uint16_t plot_h;
    uint16_t max_value;
    uint16_t column;
    uint8_t wave_idx;

    if (!server || !obj || !obj->in_use) return GUI2C_STATUS_ERR_INVALID_ARGUMENT;

    plot_x = waveform_plot_x(obj);
    plot_y = waveform_plot_y(obj);
    plot_w = waveform_plot_w(obj);
    plot_h = waveform_plot_h(obj);
    if (plot_w == 0u || plot_h == 0u) return GUI2C_STATUS_OK;

    {
        int16_t column_tops[GUI2C_MAX_WAVEFORM_WAVES][plot_w];
        int16_t column_bottoms[GUI2C_MAX_WAVEFORM_WAVES][plot_w];

    max_value = waveform_max_sample(obj);

        for (wave_idx = 0u; wave_idx < obj->wave_count; ++wave_idx) {
            waveform_prepare_wave_columns(obj,
                                          wave_idx,
                                          plot_x,
                                          plot_y,
                                          plot_w,
                                          plot_h,
                                          max_value,
                                          column_tops[wave_idx],
                                          column_bottoms[wave_idx]);
        }

        for (column = 0u; column < plot_w; ++column) {
            uint16_t draw_x = (uint16_t)(plot_x + column);
            gui2c_status_t st = gui2c_server_fill_bg(server, (uint16_t)draw_x, plot_y, 1u, plot_h, obj->bg);
            if (st != GUI2C_STATUS_OK) return st;

            for (wave_idx = 0u; wave_idx < obj->wave_count; ++wave_idx) {
                if (column_tops[wave_idx][column] < 0 || column_bottoms[wave_idx][column] < 0) continue;
                st = waveform_draw_vspan(server,
                                         (int)draw_x,
                                         column_tops[wave_idx][column],
                                         column_bottoms[wave_idx][column],
                                         obj->wave_colors[wave_idx],
                                         plot_x,
                                         plot_y,
                                         plot_w,
                                         plot_h);
                if (st != GUI2C_STATUS_OK) return st;
            }
        }

        return GUI2C_STATUS_OK;
    }

    return GUI2C_STATUS_OK;
}

static gui2c_status_t waveform_render_plot_columns_diff(gui2c_server_t *server,
                                                        const gui2c_waveform_object_t *old_obj,
                                                        const gui2c_waveform_object_t *new_obj) {
    uint16_t plot_x;
    uint16_t plot_y;
    uint16_t plot_w;
    uint16_t plot_h;
    uint16_t old_max_value;
    uint16_t new_max_value;
    uint16_t column;
    uint8_t wave_idx;

    if (!server || !old_obj || !new_obj || !new_obj->in_use) return GUI2C_STATUS_ERR_INVALID_ARGUMENT;

    plot_x = waveform_plot_x(new_obj);
    plot_y = waveform_plot_y(new_obj);
    plot_w = waveform_plot_w(new_obj);
    plot_h = waveform_plot_h(new_obj);
    if (plot_w == 0u || plot_h == 0u) return GUI2C_STATUS_OK;

    {
        int16_t old_tops[GUI2C_MAX_WAVEFORM_WAVES][plot_w];
        int16_t old_bottoms[GUI2C_MAX_WAVEFORM_WAVES][plot_w];
        int16_t new_tops[GUI2C_MAX_WAVEFORM_WAVES][plot_w];
        int16_t new_bottoms[GUI2C_MAX_WAVEFORM_WAVES][plot_w];

        old_max_value = waveform_max_sample(old_obj);
        new_max_value = waveform_max_sample(new_obj);

        for (wave_idx = 0u; wave_idx < new_obj->wave_count; ++wave_idx) {
            waveform_prepare_wave_columns(old_obj,
                                          wave_idx,
                                          plot_x,
                                          plot_y,
                                          plot_w,
                                          plot_h,
                                          old_max_value,
                                          old_tops[wave_idx],
                                          old_bottoms[wave_idx]);
            waveform_prepare_wave_columns(new_obj,
                                          wave_idx,
                                          plot_x,
                                          plot_y,
                                          plot_w,
                                          plot_h,
                                          new_max_value,
                                          new_tops[wave_idx],
                                          new_bottoms[wave_idx]);
        }

        for (column = 0u; column < plot_w; ++column) {
            int changed = 0;

            for (wave_idx = 0u; wave_idx < new_obj->wave_count; ++wave_idx) {
                if (old_tops[wave_idx][column] != new_tops[wave_idx][column] ||
                    old_bottoms[wave_idx][column] != new_bottoms[wave_idx][column]) {
                    changed = 1;
                    break;
                }
            }

            if (!changed) continue;

            {
                uint16_t draw_x = (uint16_t)(plot_x + column);
                gui2c_status_t st = gui2c_server_fill_bg(server, draw_x, plot_y, 1u, plot_h, new_obj->bg);
                if (st != GUI2C_STATUS_OK) return st;

                for (wave_idx = 0u; wave_idx < new_obj->wave_count; ++wave_idx) {
                    if (new_tops[wave_idx][column] < 0 || new_bottoms[wave_idx][column] < 0) continue;
                    st = waveform_draw_vspan(server,
                                             (int)draw_x,
                                             new_tops[wave_idx][column],
                                             new_bottoms[wave_idx][column],
                                             new_obj->wave_colors[wave_idx],
                                             plot_x,
                                             plot_y,
                                             plot_w,
                                             plot_h);
                    if (st != GUI2C_STATUS_OK) return st;
                }
            }
        }
    }

    return GUI2C_STATUS_OK;
}

gui2c_status_t gui2c_object_waveform_draw(gui2c_server_t *server, const gui2c_waveform_object_t *obj) {
    gui2c_status_t st;

    if (!server || !obj || !obj->in_use) return GUI2C_STATUS_ERR_INVALID_ARGUMENT;

    st = waveform_draw_frame(server, obj);
    if (st != GUI2C_STATUS_OK) return st;

    return waveform_render_plot_columns(server, obj);
}

gui2c_status_t gui2c_object_waveform_create(gui2c_server_t *server, const uint8_t *payload, uint16_t payload_len) {
    gui2c_waveform_create_desc_t desc;
    gui2c_waveform_object_t *obj;
    gui2c_status_t st;

    st = parse_waveform_create_desc(payload, payload_len, &desc);
    if (st != GUI2C_STATUS_OK) return st;

    obj = gui2c_server_find_waveform_object(server, desc.object_id);
    if (obj) return GUI2C_STATUS_OK;

    st = create_or_replace_waveform(server, &desc, 0);
    if (st != GUI2C_STATUS_OK) return st;

    obj = gui2c_server_find_waveform_object(server, desc.object_id);
    st = gui2c_object_waveform_draw(server, obj);
    if (st != GUI2C_STATUS_OK) return st;

    return gui2c_server_flush(server);
}

gui2c_status_t gui2c_object_waveform_update(gui2c_server_t *server, const uint8_t *payload, uint16_t payload_len) {
    gui2c_waveform_update_desc_t update;
    gui2c_waveform_object_t *obj;
    gui2c_waveform_object_t old_obj;
    gui2c_status_t st;

    st = parse_waveform_update_desc(payload, payload_len, &update);
    if (st != GUI2C_STATUS_OK) return st;

    obj = gui2c_server_find_waveform_object(server, update.object_id);
    if (!obj) return GUI2C_STATUS_ERR_NOT_FOUND;

    old_obj = *obj;

    if (update.mode == (uint8_t)GUI2C_WAVEFORM_UPDATE_MODE_FULL_REPLACEMENT) {
        st = waveform_apply_full_update(obj, &update);
    } else if (update.mode == (uint8_t)GUI2C_WAVEFORM_UPDATE_MODE_SHIFTING_MULTI) {
        st = waveform_apply_shift_multi_update(obj, &update);
    } else {
        st = waveform_apply_shift_update(obj, &update);
    }
    if (st != GUI2C_STATUS_OK) return st;

    st = waveform_render_plot_columns_diff(server, &old_obj, obj);
    if (st != GUI2C_STATUS_OK) return st;

    return gui2c_server_flush(server);
}

gui2c_status_t gui2c_object_waveform_destroy(gui2c_server_t *server, gui2c_object_id_t object_id) {
    gui2c_waveform_object_t *obj;
    gui2c_status_t st;

    obj = gui2c_server_find_waveform_object(server, object_id);
    if (!obj) return GUI2C_STATUS_ERR_NOT_FOUND;

    st = waveform_clear(server, obj);
    if (st != GUI2C_STATUS_OK) return st;

    memset(obj, 0, sizeof(*obj));
    return gui2c_server_flush(server);
}
