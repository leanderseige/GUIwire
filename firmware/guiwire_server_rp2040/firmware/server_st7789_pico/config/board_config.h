/* SPDX-License-Identifier: MIT */

#ifndef GUIWIRE_SERVER_ST7789_BOARD_CONFIG_H
#define GUIWIRE_SERVER_ST7789_BOARD_CONFIG_H

#define GUIWIRE_UART_PORT uart0
#define GUIWIRE_PIN_GUIWIRE_TX 0u
#define GUIWIRE_PIN_GUIWIRE_RX 1u
#include "guiwire/guiwire_pico_uart_pinmap.h"
#if !GUIWIRE_PICO_UART0_PINS_VALID(GUIWIRE_PIN_GUIWIRE_TX, GUIWIRE_PIN_GUIWIRE_RX)
#error "RP2040 UART0 TX/RX pins are not a valid hardware pair"
#endif
/* Active-low button to GND; firmware enables the internal pull-up. */
#define GUIWIRE_PIN_DEBUG_BUTTON 8

#define GUIWIRE_SPI_PORT spi0
#define GUIWIRE_SPI_BAUDRATE 16000000u
#define GUIWIRE_SPI_CPOL 1u
#define GUIWIRE_SPI_CPHA 1u
#define GUIWIRE_PIN_LCD_SCK  2u
#define GUIWIRE_PIN_LCD_MOSI 3u

/* The supplied wiring lists no CS pin; -1 means CS is tied active on the module. */
#define GUIWIRE_PIN_LCD_CS   (-1)
#define GUIWIRE_PIN_LCD_RST  4
#define GUIWIRE_PIN_LCD_DC   5
#define GUIWIRE_PIN_LCD_BL   (-1)

#define GUIWIRE_DISPLAY_WIDTH  240u
#define GUIWIRE_DISPLAY_HEIGHT 240u
#define GUIWIRE_DISPLAY_X_OFFSET 0u
#define GUIWIRE_DISPLAY_Y_OFFSET 0u
#define GUIWIRE_DISPLAY_ROTATION 0u
#define GUIWIRE_DISPLAY_INVERT true
#define GUIWIRE_DISPLAY_BGR true

#endif
