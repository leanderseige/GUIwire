#include <stdio.h>
#include <string.h>

#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/i2c.h"
#include "hardware/spi.h"
#include "hardware/irq.h"
#include "hardware/structs/i2c.h"
#include "hardware/regs/i2c.h"

#include "gui2c/gui2c.h"
#include "platform/pico/gui2c_platform_pico.h"
#include "board_config.h"

typedef struct {
    gui2c_pico_spi_bus_t spi_bus;
} server_hal_t;

#define GUI2C_I2C_RX_QUEUE_LEN 4u

typedef struct {
    uint16_t len;
    uint8_t data[GUI2C_MAX_PACKET_SIZE];
} gui2c_i2c_packet_t;

static volatile gui2c_i2c_packet_t g_i2c_rx_queue[GUI2C_I2C_RX_QUEUE_LEN];
static volatile uint8_t g_i2c_rx_head = 0u;
static volatile uint8_t g_i2c_rx_tail = 0u;

static volatile uint8_t g_i2c_acc_buf[GUI2C_MAX_PACKET_SIZE];
static volatile uint16_t g_i2c_acc_len = 0u;
static volatile bool g_i2c_acc_overflow = false;
static volatile uint32_t g_i2c_packets_dropped = 0u;

static void board_init_spi(void) {
    spi_init(GUI2C_SPI_PORT, GUI2C_SPI_BAUDRATE);
    gpio_set_function(GUI2C_PIN_LCD_MOSI, GPIO_FUNC_SPI);
    gpio_set_function(GUI2C_PIN_LCD_SCK, GPIO_FUNC_SPI);
    gpio_set_function(GUI2C_PIN_LCD_MISO, GPIO_FUNC_SPI);

    gpio_init(GUI2C_PIN_LCD_CS);
    gpio_set_dir(GUI2C_PIN_LCD_CS, GPIO_OUT);
    gpio_put(GUI2C_PIN_LCD_CS, 1);

    gpio_init(GUI2C_PIN_LCD_DC);
    gpio_set_dir(GUI2C_PIN_LCD_DC, GPIO_OUT);
    gpio_put(GUI2C_PIN_LCD_DC, 1);

    gpio_init(GUI2C_PIN_LCD_RST);
    gpio_set_dir(GUI2C_PIN_LCD_RST, GPIO_OUT);
    gpio_put(GUI2C_PIN_LCD_RST, 1);

    gpio_init(GUI2C_PIN_LCD_BL);
    gpio_set_dir(GUI2C_PIN_LCD_BL, GPIO_OUT);
    gpio_put(GUI2C_PIN_LCD_BL, 0);
}

static void board_i2c_slave_irq(void) {
    i2c_hw_t *hw = i2c_get_hw(GUI2C_I2C_PORT);

    while (hw->status & I2C_IC_STATUS_RFNE_BITS) {
        uint8_t b = (uint8_t)(hw->data_cmd & 0xFFu);

        if (!g_i2c_acc_overflow) {
            if (g_i2c_acc_len < GUI2C_MAX_PACKET_SIZE) {
                g_i2c_acc_buf[g_i2c_acc_len++] = b;
            } else {
                g_i2c_acc_overflow = true;
            }
        }
    }

    if (hw->raw_intr_stat & I2C_IC_RAW_INTR_STAT_STOP_DET_BITS) {
        uint8_t next_head;

        (void)hw->clr_stop_det;

        if (g_i2c_acc_overflow) {
            g_i2c_acc_len = 0u;
            g_i2c_acc_overflow = false;
            g_i2c_packets_dropped++;
            return;
        }

        if (g_i2c_acc_len == 0u) {
            return;
        }

        next_head = (uint8_t)((g_i2c_rx_head + 1u) % GUI2C_I2C_RX_QUEUE_LEN);
        if (next_head == g_i2c_rx_tail) {
            g_i2c_acc_len = 0u;
            g_i2c_packets_dropped++;
            return;
        }

        g_i2c_rx_queue[g_i2c_rx_head].len = g_i2c_acc_len;
        memcpy((void *)g_i2c_rx_queue[g_i2c_rx_head].data,
               (const void *)g_i2c_acc_buf,
               g_i2c_acc_len);

        g_i2c_rx_head = next_head;
        g_i2c_acc_len = 0u;
    }
}

static void board_init_i2c_slave(void) {
    i2c_hw_t *hw = i2c_get_hw(GUI2C_I2C_PORT);
    int irq_num = (GUI2C_I2C_PORT == i2c0) ? I2C0_IRQ : I2C1_IRQ;

    i2c_init(GUI2C_I2C_PORT, GUI2C_I2C_BAUDRATE);
    gpio_set_function(GUI2C_PIN_I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(GUI2C_PIN_I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(GUI2C_PIN_I2C_SDA);
    gpio_pull_up(GUI2C_PIN_I2C_SCL);
    i2c_set_slave_mode(GUI2C_I2C_PORT, true, GUI2C_I2C_ADDRESS);

    g_i2c_rx_head = 0u;
    g_i2c_rx_tail = 0u;
    g_i2c_acc_len = 0u;
    g_i2c_acc_overflow = false;
    g_i2c_packets_dropped = 0u;

    (void)hw->clr_intr;
    hw->intr_mask = I2C_IC_INTR_MASK_M_RX_FULL_BITS | I2C_IC_INTR_MASK_M_STOP_DET_BITS;

    irq_set_exclusive_handler(irq_num, board_i2c_slave_irq);
    irq_set_enabled(irq_num, true);
}

static bool board_i2c_slave_pop_packet(uint8_t *buffer, size_t capacity, size_t *out_len) {
    uint8_t tail;
    uint16_t len;
    int irq_num;

    if (!buffer || !out_len) return false;

    irq_num = (GUI2C_I2C_PORT == i2c0) ? I2C0_IRQ : I2C1_IRQ;
    irq_set_enabled(irq_num, false);

    if (g_i2c_rx_tail == g_i2c_rx_head) {
        irq_set_enabled(irq_num, true);
        return false;
    }

    tail = g_i2c_rx_tail;
    len = g_i2c_rx_queue[tail].len;

    if (len > capacity) {
        g_i2c_rx_tail = (uint8_t)((g_i2c_rx_tail + 1u) % GUI2C_I2C_RX_QUEUE_LEN);
        irq_set_enabled(irq_num, true);
        return false;
    }

    memcpy(buffer, (const void *)g_i2c_rx_queue[tail].data, len);
    g_i2c_rx_tail = (uint8_t)((g_i2c_rx_tail + 1u) % GUI2C_I2C_RX_QUEUE_LEN);

    irq_set_enabled(irq_num, true);

    *out_len = len;
    return true;
}

int main(void) {
    gui2c_server_t server;
    gui2c_server_config_t server_cfg = {0};
    gui2c_display_ili9488_t display;
    gui2c_display_ili9488_config_t display_cfg = {0};
    server_hal_t hal;
    uint8_t rxbuf[GUI2C_MAX_PACKET_SIZE];

    stdio_init_all();
    sleep_ms(200);
    printf("GUI2C server starting\n");

    board_init_spi();
    board_init_i2c_slave();

    hal.spi_bus.spi = GUI2C_SPI_PORT;

    display_cfg.spi_write     = gui2c_platform_pico_spi_write;
    display_cfg.gpio_write    = gui2c_platform_pico_gpio_write;
    display_cfg.delay_ms      = gui2c_platform_pico_delay_ms;
    display_cfg.user_ctx      = &hal.spi_bus;
    display_cfg.width         = GUI2C_DISPLAY_WIDTH;
    display_cfg.height        = GUI2C_DISPLAY_HEIGHT;
    display_cfg.phys_width    = 320u;
    display_cfg.phys_height   = 480u;
    display_cfg.rotation      = GUI2C_DISPLAY_ROTATION;
    display_cfg.invert_colors = false;
    display_cfg.bgr           = true;
    display_cfg.pin_cs        = GUI2C_PIN_LCD_CS;
    display_cfg.pin_dc        = GUI2C_PIN_LCD_DC;
    display_cfg.pin_rst       = GUI2C_PIN_LCD_RST;
    display_cfg.pin_bl        = GUI2C_PIN_LCD_BL;

    if (gui2c_display_ili9488_init(&display, &display_cfg) != GUI2C_STATUS_OK) {
        while (1) sleep_ms(1000);
    }

    server_cfg.display_size.width  = GUI2C_DISPLAY_WIDTH;
    server_cfg.display_size.height = GUI2C_DISPLAY_HEIGHT;
    server_cfg.renderer            = &gui2c_display_ili9488_renderer;
    server_cfg.renderer_ctx        = gui2c_display_ili9488_context(&display);
    server_cfg.server_id           = "GUI2C SERVER";

    if (gui2c_server_init(&server, &server_cfg) != GUI2C_STATUS_OK) {
        while (1) sleep_ms(1000);
    }

    gui2c_server_show_waiting(&server);

    while (1) {
        size_t packet_len;

        if (board_i2c_slave_pop_packet(rxbuf, sizeof(rxbuf), &packet_len)) {
            gui2c_status_t st = gui2c_server_handle_packet(&server, rxbuf, packet_len);
            printf("Received packet (%u bytes): %s\n", (unsigned)packet_len, gui2c_status_str(st));
        } else {
            tight_loop_contents();
        }
    }
}