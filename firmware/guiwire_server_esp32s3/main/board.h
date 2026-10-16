/* SPDX-License-Identifier: MIT */
#ifndef GUIWIRE_ESP32_BOARD_H
#define GUIWIRE_ESP32_BOARD_H

#include "esp_err.h"
#include "guiwire/guiwire_framebuffer.h"

#define GUIWIRE_BOARD_WIDTH 600u
#define GUIWIRE_BOARD_HEIGHT 450u

/* GPIO7/8 are free header pins on both PCB revisions. UART1 avoids ROM and
 * bootloader text on GPIO43/44 (the default UART0 console pins). */
#define GUIWIRE_BOARD_UART_TX 7
#define GUIWIRE_BOARD_UART_RX 8

esp_err_t guiwire_board_init(void);
guiwire_color_t *guiwire_board_alloc_framebuffer(void);
guiwire_status_t guiwire_board_present(void *ctx, uint16_t x, uint16_t y,
                                       uint16_t width, uint16_t height,
                                       const guiwire_color_t *pixels,
                                       uint16_t stride);

#endif
