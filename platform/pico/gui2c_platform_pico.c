#include "platform/pico/gui2c_platform_pico.h"

gui2c_status_t gui2c_platform_pico_i2c_write(void *ctx, const uint8_t *data, size_t len) {
    gui2c_pico_i2c_controller_t *bus = (gui2c_pico_i2c_controller_t *)ctx;
    int rc;
    if (!bus || !bus->i2c || !data) return GUI2C_STATUS_ERR_INVALID_ARGUMENT;
    rc = i2c_write_blocking(bus->i2c, bus->address, data, len, false);
    return (rc < 0) ? GUI2C_STATUS_ERR_TRANSPORT : GUI2C_STATUS_OK;
}

gui2c_status_t gui2c_platform_pico_spi_write(void *ctx, const uint8_t *data, size_t len) {
    gui2c_pico_spi_bus_t *bus = (gui2c_pico_spi_bus_t *)ctx;
    if (!bus || !bus->spi || !data) return GUI2C_STATUS_ERR_INVALID_ARGUMENT;
    spi_write_blocking(bus->spi, data, len);
    return GUI2C_STATUS_OK;
}

void gui2c_platform_pico_gpio_write(void *ctx, int pin, bool level) {
    (void)ctx;
    if (pin >= 0) gpio_put((uint)pin, level ? 1u : 0u);
}

void gui2c_platform_pico_delay_ms(void *ctx, uint32_t ms) {
    (void)ctx;
    sleep_ms(ms);
}
