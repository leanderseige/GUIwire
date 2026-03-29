#ifndef GUI2C_DISPLAY_H
#define GUI2C_DISPLAY_H

#include "gui2c/gui2c_renderer.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef gui2c_status_t (*gui2c_spi_write_fn)(void *ctx, const uint8_t *data, size_t len);
typedef void (*gui2c_gpio_write_fn)(void *ctx, int pin, bool level);
typedef void (*gui2c_delay_ms_fn)(void *ctx, uint32_t ms);

#ifdef __cplusplus
}
#endif

#endif
