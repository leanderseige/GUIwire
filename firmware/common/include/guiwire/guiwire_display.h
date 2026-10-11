/*
 * SPDX-License-Identifier: MIT
 * Authors: any:frequency, seige.digital GbR, Leander Seige (2026)
 */

#ifndef GUIWIRE_DISPLAY_H
#define GUIWIRE_DISPLAY_H

#include "guiwire/guiwire_renderer.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef guiwire_status_t (*guiwire_spi_write_fn)(void *ctx, const uint8_t *data, size_t len);
typedef void (*guiwire_gpio_write_fn)(void *ctx, int pin, bool level);
typedef void (*guiwire_delay_ms_fn)(void *ctx, uint32_t ms);

#ifdef __cplusplus
}
#endif

#endif
