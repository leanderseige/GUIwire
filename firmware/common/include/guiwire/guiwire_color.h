/*
 * SPDX-License-Identifier: MIT
 * Authors: any:frequency, seige.digital GbR, Leander Seige (2026)
 */

#ifndef GUIWIRE_COLOR_H
#define GUIWIRE_COLOR_H

#include <stdint.h>
#include "guiwire/guiwire_types.h"

#ifdef __cplusplus
extern "C" {
#endif

guiwire_color_t guiwire_rgb565(uint8_t r, uint8_t g, uint8_t b);

#ifdef __cplusplus
}
#endif

#endif
