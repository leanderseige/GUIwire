/* SPDX-License-Identifier: MIT */

#ifndef GUIWIRE_DEMO_CLIENT_RP2040_BOARD_CONFIG_H
#define GUIWIRE_DEMO_CLIENT_RP2040_BOARD_CONFIG_H

#define GUIWIRE_UART_PORT uart0
#define GUIWIRE_PIN_GUIWIRE_TX 0u
#define GUIWIRE_PIN_GUIWIRE_RX 1u
#include "guiwire/guiwire_pico_uart_pinmap.h"
#if !GUIWIRE_PICO_UART0_PINS_VALID(GUIWIRE_PIN_GUIWIRE_TX, GUIWIRE_PIN_GUIWIRE_RX)
#error "RP2040 UART0 TX/RX pins are not a valid hardware pair"
#endif
#define GUIWIRE_UART_BAUD 9600u

#endif
