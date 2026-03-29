#ifndef GUI2C_TRANSPORT_H
#define GUI2C_TRANSPORT_H

#include <stddef.h>
#include <stdint.h>
#include "gui2c/gui2c_status.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef gui2c_status_t (*gui2c_i2c_write_fn)(void *ctx, const uint8_t *data, size_t len);

typedef struct {
    gui2c_i2c_write_fn write;
    void *ctx;
} gui2c_transport_t;

#ifdef __cplusplus
}
#endif

#endif
