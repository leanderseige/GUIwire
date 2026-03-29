#ifndef GUI2C_TYPES_H
#define GUI2C_TYPES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint16_t gui2c_color_t;

typedef struct {
    uint16_t width;
    uint16_t height;
} gui2c_size_t;

#ifdef __cplusplus
}
#endif

#endif
