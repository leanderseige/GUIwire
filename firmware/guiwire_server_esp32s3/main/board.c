/* SPDX-License-Identifier: MIT */
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "driver/spi_master.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_sh8601.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include "board.h"

#define LCD_SPI_HOST SPI2_HOST
#define LCD_CS GPIO_NUM_9
#define LCD_CLK GPIO_NUM_10
#define LCD_D0 GPIO_NUM_11
#define LCD_D1 GPIO_NUM_12
#define LCD_D2 GPIO_NUM_13
#define LCD_D3 GPIO_NUM_14
#define LCD_Y_OFFSET 16u
#define LCD_DMA_ROWS 12u
#define I2C_PORT I2C_NUM_0
#define EXPANDER_ADDRESS 0x20u

static esp_lcd_panel_handle_t panel;
static SemaphoreHandle_t color_done;
static uint8_t *dma_pixels;
static bool panel_faulted;

/* Board-specific RM690B0 setup used with Espressif's QSPI SH8601 transport.
 * The panel driver owns CASET/PASET for every draw; no stale window is kept. */
static const sh8601_lcd_init_cmd_t lcd_init_cmds[] = {
    {0xFE, (uint8_t[]){0x20}, 1, 0},
    {0x26, (uint8_t[]){0x0A}, 1, 0},
    {0x24, (uint8_t[]){0x80}, 1, 0},
    {0xFE, (uint8_t[]){0x00}, 1, 0},
    {0x3A, (uint8_t[]){0x55}, 1, 0},
    {0xC2, (uint8_t[]){0x00}, 1, 10},
    {0x35, (uint8_t[]){0x00}, 0, 0},
    {0x51, (uint8_t[]){0x00}, 1, 10},
    {0x11, (uint8_t[]){0x00}, 0, 80},
    {0x29, (uint8_t[]){0x00}, 0, 10},
    {0x36, (uint8_t[]){0x30}, 1, 0},
    {0x51, (uint8_t[]){0xFF}, 1, 0},
};

static bool color_trans_done(esp_lcd_panel_io_handle_t io,
                             esp_lcd_panel_io_event_data_t *event,
                             void *user_ctx) {
    BaseType_t higher_priority_woken = pdFALSE;
    (void)io;
    (void)event;
    xSemaphoreGiveFromISR((SemaphoreHandle_t)user_ctx, &higher_priority_woken);
    return higher_priority_woken == pdTRUE;
}

static esp_err_t expander_read(uint8_t reg, uint8_t *value) {
    return i2c_master_write_read_device(I2C_PORT, EXPANDER_ADDRESS, &reg, 1u,
                                         value, 1u, pdMS_TO_TICKS(100));
}

static esp_err_t expander_write(uint8_t reg, uint8_t value) {
    uint8_t command[2] = {reg, value};
    return i2c_master_write_to_device(I2C_PORT, EXPANDER_ADDRESS, command,
                                       sizeof(command), pdMS_TO_TICKS(100));
}

static esp_err_t expander_output(uint8_t pin, bool high) {
    uint8_t output;
    esp_err_t err = expander_read(1u, &output);
    if (err != ESP_OK) return err;
    output = high ? (uint8_t)(output | (1u << pin))
                  : (uint8_t)(output & ~(1u << pin));
    return expander_write(1u, output);
}

static esp_err_t expander_set_output(uint8_t pin, bool initial_high) {
    uint8_t config;
    esp_err_t err = expander_output(pin, initial_high);
    if (err != ESP_OK) return err;
    err = expander_read(3u, &config);
    if (err != ESP_OK) return err;
    return expander_write(3u, (uint8_t)(config & ~(1u << pin)));
}

static esp_err_t board_i2c_init(void) {
    i2c_config_t config = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = GPIO_NUM_47,
        .scl_io_num = GPIO_NUM_48,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 100000u,
    };
    esp_err_t err = i2c_param_config(I2C_PORT, &config);
    if (err != ESP_OK) return err;
    return i2c_driver_install(I2C_PORT, config.mode, 0u, 0u, 0u);
}

static esp_err_t board_release_display(void) {
    esp_err_t err = board_i2c_init();
    if (err != ESP_OK) return err;
#if CONFIG_GUIWIRE_BOARD_V2
    err = expander_set_output(0u, true); /* EXIO0 = AMOLED reset */
    if (err != ESP_OK) return err;
    vTaskDelay(pdMS_TO_TICKS(20));
    err = expander_output(0u, false);
    if (err != ESP_OK) return err;
    vTaskDelay(pdMS_TO_TICKS(20));
    err = expander_output(0u, true);
#else
    /* V1: EXIO1 enables panel power; reset is a direct GPIO21. */
    err = expander_set_output(1u, true);
    if (err != ESP_OK) return err;
    gpio_config_t reset = {
        .pin_bit_mask = 1ULL << GPIO_NUM_21,
        .mode = GPIO_MODE_OUTPUT,
    };
    err = gpio_config(&reset);
    if (err != ESP_OK) return err;
    err = gpio_set_level(GPIO_NUM_21, 1);
    if (err != ESP_OK) return err;
    vTaskDelay(pdMS_TO_TICKS(20));
    err = gpio_set_level(GPIO_NUM_21, 0);
    if (err != ESP_OK) return err;
    vTaskDelay(pdMS_TO_TICKS(20));
    err = gpio_set_level(GPIO_NUM_21, 1);
#endif
    if (err == ESP_OK) vTaskDelay(pdMS_TO_TICKS(120));
    return err;
}

esp_err_t guiwire_board_init(void) {
    esp_err_t err = board_release_display();
    if (err != ESP_OK) return err;

    color_done = xSemaphoreCreateBinary();
    dma_pixels = heap_caps_malloc(GUIWIRE_BOARD_WIDTH * LCD_DMA_ROWS * 2u,
                                  MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    if (!color_done || !dma_pixels) return ESP_ERR_NO_MEM;

    spi_bus_config_t bus_cfg = SH8601_PANEL_BUS_QSPI_CONFIG(
        LCD_CLK, LCD_D0, LCD_D1, LCD_D2, LCD_D3,
        GUIWIRE_BOARD_WIDTH * LCD_DMA_ROWS * 2u);
    err = spi_bus_initialize(LCD_SPI_HOST, &bus_cfg, SPI_DMA_CH_AUTO);
    if (err != ESP_OK) return err;

    esp_lcd_panel_io_spi_config_t io_cfg = SH8601_PANEL_IO_QSPI_CONFIG(
        LCD_CS, color_trans_done, color_done);
    esp_lcd_panel_io_handle_t io;
    err = esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_SPI_HOST,
                                    &io_cfg, &io);
    if (err != ESP_OK) return err;

    sh8601_vendor_config_t vendor_cfg = {
        .init_cmds = lcd_init_cmds,
        .init_cmds_size = sizeof(lcd_init_cmds) / sizeof(lcd_init_cmds[0]),
        .flags = {.use_qspi_interface = 1},
    };
    esp_lcd_panel_dev_config_t panel_cfg = {
        .reset_gpio_num = -1,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
        .vendor_config = &vendor_cfg,
    };
    err = esp_lcd_new_panel_sh8601(io, &panel_cfg, &panel);
    if (err != ESP_OK) return err;
    err = esp_lcd_panel_reset(panel);
    if (err != ESP_OK) return err;
    err = esp_lcd_panel_init(panel);
    if (err != ESP_OK) return err;
    /* Waveshare's landscape setup uses MADCTL=0x30 in lcd_init_cmds and a
     * 16-pixel Y offset. Applying another rotation here would undo it. */
    return esp_lcd_panel_disp_on_off(panel, true);
}

guiwire_color_t *guiwire_board_alloc_framebuffer(void) {
    return heap_caps_calloc((size_t)GUIWIRE_BOARD_WIDTH * GUIWIRE_BOARD_HEIGHT,
                            sizeof(guiwire_color_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
}

guiwire_status_t guiwire_board_present(void *ctx, uint16_t x, uint16_t y,
                                       uint16_t width, uint16_t height,
                                       const guiwire_color_t *pixels,
                                       uint16_t stride) {
    (void)ctx;
    if (panel_faulted) return GUIWIRE_STATUS_ERR_TRANSPORT;
    if (!panel || !pixels || !dma_pixels || !width || !height ||
        !stride || height > 8u) return GUIWIRE_STATUS_ERR_INVALID_ARGUMENT;
    /* The QSPI driver requires even window bounds. Expand by at most one
     * pixel each way and copy the already rendered neighbours from PSRAM. */
    uint16_t x0 = x & ~1u;
    uint16_t y0 = y & ~1u;
    uint16_t x1 = (uint16_t)((x + width + 1u) & ~1u);
    uint16_t y1 = (uint16_t)((y + height + 1u) & ~1u);
    if (x1 > GUIWIRE_BOARD_WIDTH) x1 = GUIWIRE_BOARD_WIDTH;
    if (y1 > GUIWIRE_BOARD_HEIGHT) y1 = GUIWIRE_BOARD_HEIGHT;
    const guiwire_color_t *base = pixels - (size_t)y * stride - x;
    for (uint16_t row = y0; row < y1; ++row) {
        for (uint16_t col = x0; col < x1; ++col) {
            guiwire_color_t color = base[(size_t)row * stride + col];
            size_t offset = ((size_t)(row - y0) * (x1 - x0) + (col - x0)) * 2u;
            dma_pixels[offset] = (uint8_t)(color >> 8);
            dma_pixels[offset + 1u] = (uint8_t)color;
        }
    }
    esp_err_t err = esp_lcd_panel_draw_bitmap(panel, x0, y0 + LCD_Y_OFFSET,
                                               x1, y1 + LCD_Y_OFFSET,
                                               dma_pixels);
    if (err != ESP_OK) {
        panel_faulted = true;
        return GUIWIRE_STATUS_ERR_TRANSPORT;
    }
    /* draw_bitmap is asynchronous. Do not overwrite the DMA staging buffer
     * until the panel IO confirms completion. */
    if (xSemaphoreTake(color_done, pdMS_TO_TICKS(1000)) != pdTRUE) {
        /* A previous DMA transaction may still own dma_pixels. Do not reuse
         * it until the board is rebooted or explicitly reinitialized. */
        panel_faulted = true;
        return GUIWIRE_STATUS_ERR_TRANSPORT;
    }
    return GUIWIRE_STATUS_OK;
}
