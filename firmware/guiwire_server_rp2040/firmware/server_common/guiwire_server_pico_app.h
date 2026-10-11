/* SPDX-License-Identifier: MIT */

#ifndef GUIWIRE_SERVER_PICO_APP_H
#define GUIWIRE_SERVER_PICO_APP_H

#include "hardware/uart.h"
#include "guiwire/guiwire_server.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    guiwire_size_t display_size;
    const guiwire_renderer_vtable_t *renderer;
    void *renderer_ctx;
    const char *server_id;
    uart_inst_t *uart;
    uint tx_pin;
    uint rx_pin;
    int debug_button_pin;
} guiwire_server_pico_app_config_t;

int guiwire_server_pico_app_run(const guiwire_server_pico_app_config_t *cfg);

#ifdef __cplusplus
}
#endif

#endif
