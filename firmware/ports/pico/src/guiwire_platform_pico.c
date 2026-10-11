/*
 * SPDX-License-Identifier: MIT
 * Authors: any:frequency, seige.digital GbR, Leander Seige (2026)
 */

#include "guiwire/guiwire_platform_pico.h"

void guiwire_platform_pico_uart_init(guiwire_pico_uart_t *bus,
                                     uart_inst_t *uart,
                                     uint32_t baudrate,
                                     uint tx_pin,
                                     uint rx_pin) {
    if (!bus || !uart) return;
    bus->uart = uart;
    bus->rx_captured_bytes = 0u;
    bus->rx_captured_sync_bytes = 0u;
    bus->rx_last_captured_byte = 0u;
    bus->rx_flushed_bytes = 0u;
    bus->rx_flushed_sync_bytes = 0u;
    bus->rx_overflow_bytes = 0u;
    uart_init(uart, baudrate);
    gpio_set_function(tx_pin, GPIO_FUNC_UART);
    gpio_set_function(rx_pin, GPIO_FUNC_UART);
    uart_set_format(uart, 8u, 1u, UART_PARITY_NONE);
    uart_set_hw_flow(uart, false, false);
    uart_set_fifo_enabled(uart, true);
}

guiwire_status_t guiwire_platform_pico_uart_set_baud(void *ctx, uint32_t baudrate) {
    guiwire_pico_uart_t *bus = (guiwire_pico_uart_t *)ctx;
    if (!bus || !bus->uart || !baudrate) return GUIWIRE_STATUS_ERR_INVALID_ARGUMENT;
    return uart_set_baudrate(bus->uart, baudrate) ?
        GUIWIRE_STATUS_OK : GUIWIRE_STATUS_ERR_INVALID_ARGUMENT;
}

guiwire_status_t guiwire_platform_pico_uart_write_byte(void *ctx, uint8_t byte) {
    guiwire_pico_uart_t *bus = (guiwire_pico_uart_t *)ctx;
    if (!bus || !bus->uart) return GUIWIRE_STATUS_ERR_INVALID_ARGUMENT;
    uart_putc_raw(bus->uart, byte);
    return GUIWIRE_STATUS_OK;
}

bool guiwire_platform_pico_uart_readable(const guiwire_pico_uart_t *bus) {
    return bus && bus->uart && uart_is_readable(bus->uart);
}

uint8_t guiwire_platform_pico_uart_read_byte(guiwire_pico_uart_t *bus) {
    return (bus && bus->uart) ? uart_getc(bus->uart) : 0u;
}

void guiwire_platform_pico_uart_flush_rx(guiwire_pico_uart_t *bus) {
    if (!bus || !bus->uart) return;
    while (uart_is_readable(bus->uart)) {
        (void)uart_getc(bus->uart);
        bus->rx_flushed_bytes++;
    }
}

guiwire_status_t guiwire_platform_pico_spi_write(void *ctx, const uint8_t *data, size_t len) {
    guiwire_pico_spi_bus_t *bus = (guiwire_pico_spi_bus_t *)ctx;
    if (!bus || !bus->spi || (!data && len != 0u)) return GUIWIRE_STATUS_ERR_INVALID_ARGUMENT;
    if (len != 0u) spi_write_blocking(bus->spi, data, len);
    return GUIWIRE_STATUS_OK;
}

void guiwire_platform_pico_gpio_write(void *ctx, int pin, bool level) {
    (void)ctx;
    if (pin >= 0) gpio_put((uint)pin, level ? 1u : 0u);
}

void guiwire_platform_pico_delay_ms(void *ctx, uint32_t ms) {
    (void)ctx;
    sleep_ms(ms);
}
