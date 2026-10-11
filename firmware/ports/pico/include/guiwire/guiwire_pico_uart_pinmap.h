/* SPDX-License-Identifier: MIT */

#ifndef GUIWIRE_PICO_UART_PINMAP_H
#define GUIWIRE_PICO_UART_PINMAP_H

/* RP2040 UART pin functions are fixed by the IO mux. Setting GPIO_FUNC_UART
 * does not make an arbitrary GPIO a TX or RX pin. Keep these usable from a
 * preprocessor #if so board profiles fail at build time if they are reversed. */
#define GUIWIRE_PICO_UART0_PINS_VALID(tx, rx) \
    (((tx) == 0u && (rx) == 1u) || \
     ((tx) == 12u && (rx) == 13u) || \
     ((tx) == 16u && (rx) == 17u) || \
     ((tx) == 28u && (rx) == 29u))

#define GUIWIRE_PICO_UART1_PINS_VALID(tx, rx) \
    (((tx) == 4u && (rx) == 5u) || \
     ((tx) == 8u && (rx) == 9u) || \
     ((tx) == 20u && (rx) == 21u) || \
     ((tx) == 24u && (rx) == 25u))

#endif
