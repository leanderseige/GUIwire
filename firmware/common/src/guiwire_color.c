/*
 * SPDX-License-Identifier: MIT
 * Authors: any:frequency, seige.digital GbR, Leander Seige (2026)
 */

#include "guiwire/guiwire_color.h"

guiwire_color_t guiwire_rgb565(uint8_t r, uint8_t g, uint8_t b) {
    return (guiwire_color_t)(((r & 0xF8u) << 8) | ((g & 0xFCu) << 3) | ((b & 0xF8u) >> 3));
}
