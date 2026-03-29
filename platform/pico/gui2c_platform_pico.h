#ifndef GUI2C_PLATFORM_PICO_H
#define GUI2C_PLATFORM_PICO_H

#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/spi.h"

#include "gui2c/gui2c.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    i2c_inst_t *i2c;
    uint8_t address;
} gui2c_pico_i2c_controller_t;

typedef struct {
    spi_inst_t *spi;
} gui2c_pico_spi_bus_t;

gui2c_status_t gui2c_platform_pico_i2c_write(void *ctx, const uint8_t *data, size_t len);
gui2c_status_t gui2c_platform_pico_spi_write(void *ctx, const uint8_t *data, size_t len);
void gui2c_platform_pico_gpio_write(void *ctx, int pin, bool level);
void gui2c_platform_pico_delay_ms(void *ctx, uint32_t ms);

#ifdef __cplusplus
}
#endif

#endif
