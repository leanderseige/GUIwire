/*
 * SPDX-License-Identifier: MIT
 * Authors: any:frequency, seige.digital GbR, Leander Seige (2026)
 */

#ifndef GUIWIRE_TYPES_H
#define GUIWIRE_TYPES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint16_t guiwire_color_t;

typedef struct {
    uint16_t width;
    uint16_t height;
} guiwire_size_t;

#ifdef __cplusplus
}
#endif

#endif
