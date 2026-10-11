/*
 * SPDX-License-Identifier: MIT
 * Authors: any:frequency, seige.digital GbR, Leander Seige (2026)
 */

#ifndef GUIWIRE_PLATFORM_PICO_H
#define GUIWIRE_PLATFORM_PICO_H

#include <stdbool.h>
#include <stdint.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/spi.h"
#include "hardware/uart.h"
#include "guiwire/guiwire_status.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uart_inst_t *uart;
    volatile uint32_t rx_captured_bytes;
    volatile uint32_t rx_captured_sync_bytes;
    volatile uint8_t rx_last_captured_byte;
    volatile uint32_t rx_flushed_bytes;
    volatile uint32_t rx_flushed_sync_bytes;
    volatile uint32_t rx_overflow_bytes;
} guiwire_pico_uart_t;

typedef struct {
    spi_inst_t *spi;
} guiwire_pico_spi_bus_t;

void guiwire_platform_pico_uart_init(guiwire_pico_uart_t *bus,
                                     uart_inst_t *uart,
                                     uint32_t baudrate,
                                     uint tx_pin,
                                     uint rx_pin);
guiwire_status_t guiwire_platform_pico_uart_set_baud(void *ctx, uint32_t baudrate);
guiwire_status_t guiwire_platform_pico_uart_write_byte(void *ctx, uint8_t byte);
bool guiwire_platform_pico_uart_readable(const guiwire_pico_uart_t *bus);
uint8_t guiwire_platform_pico_uart_read_byte(guiwire_pico_uart_t *bus);
void guiwire_platform_pico_uart_flush_rx(guiwire_pico_uart_t *bus);

guiwire_status_t guiwire_platform_pico_spi_write(void *ctx, const uint8_t *data, size_t len);
void guiwire_platform_pico_gpio_write(void *ctx, int pin, bool level);
void guiwire_platform_pico_delay_ms(void *ctx, uint32_t ms);

#ifdef __cplusplus
}
#endif

#endif
