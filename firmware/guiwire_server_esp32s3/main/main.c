/* SPDX-License-Identifier: MIT */
#include "driver/uart.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_rom_sys.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "guiwire/guiwire_host.h"
#include "sdkconfig.h"
#include "board.h"

#define LINK_UART UART_NUM_1
#define LINK_BASE_BAUD 9600u
#define RX_RING_SIZE 4096u

static const char *TAG = "guiwire";
#if CONFIG_GUIWIRE_ADAPTIVE_BAUD
static const uint32_t supported_bauds[] = {9600u, 19200u, 38400u, 57600u};
#endif

typedef struct {
    uint8_t ring[RX_RING_SIZE];
    uint16_t head;
    uint16_t tail;
    uint32_t epoch;
    bool tx_active;
    bool tx_started;
    uint32_t overflow;
} link_uart_t;

static link_uart_t link_uart;
static portMUX_TYPE rx_lock = portMUX_INITIALIZER_UNLOCKED;
static guiwire_host_t host;
static guiwire_framebuffer_t framebuffer;

static uint32_t time_ms(void *ctx) {
    (void)ctx;
    return (uint32_t)(esp_timer_get_time() / 1000);
}

static void new_session_token(void *ctx,
                              uint8_t token[GUIWIRE_SESSION_TOKEN_SIZE]) {
    (void)ctx;
    esp_fill_random(token, GUIWIRE_SESSION_TOKEN_SIZE);
}

static void uart_receive_task(void *ctx) {
    link_uart_t *uart = ctx;
    uint8_t bytes[128];
    for (;;) {
        portENTER_CRITICAL(&rx_lock);
        uint32_t epoch = uart->epoch;
        portEXIT_CRITICAL(&rx_lock);
        int count = uart_read_bytes(LINK_UART, bytes, sizeof(bytes), 0u);
        if (count <= 0) {
            vTaskDelay(1u);
            continue;
        }
        portENTER_CRITICAL(&rx_lock);
        if (!uart->tx_active && uart->epoch == epoch) {
            for (int i = 0; i < count; ++i) {
                uint16_t next = (uint16_t)((uart->head + 1u) & (RX_RING_SIZE - 1u));
                if (next == uart->tail) {
                    uart->overflow++;
                    break;
                }
                uart->ring[uart->head] = bytes[i];
                uart->head = next;
            }
        }
        portEXIT_CRITICAL(&rx_lock);
    }
}

static bool uart_pop_byte(link_uart_t *uart, uint8_t *byte) {
    bool available;
    portENTER_CRITICAL(&rx_lock);
    available = uart->head != uart->tail;
    if (available) {
        *byte = uart->ring[uart->tail];
        uart->tail = (uint16_t)((uart->tail + 1u) & (RX_RING_SIZE - 1u));
    }
    portEXIT_CRITICAL(&rx_lock);
    return available;
}

static guiwire_status_t uart_write_byte(void *ctx, uint8_t byte) {
    link_uart_t *uart = ctx;
    if (!uart->tx_started) {
        portENTER_CRITICAL(&rx_lock);
        uart->tx_active = true;
        uart->epoch++;
        uart->tail = uart->head;
        portEXIT_CRITICAL(&rx_lock);
        uart_flush_input(LINK_UART);
        uart->tx_started = true;
    }
    return uart_write_bytes(LINK_UART, &byte, 1u) == 1
        ? GUIWIRE_STATUS_OK : GUIWIRE_STATUS_ERR_TRANSPORT;
}

static void uart_tx_complete(void *ctx) {
    link_uart_t *uart = ctx;
    if (!uart->tx_started) return;
    if (uart_wait_tx_done(LINK_UART, pdMS_TO_TICKS(1000)) != ESP_OK) {
        ESP_LOGE(TAG, "GUIwire UART TX timeout");
    }
    /* The one-wire transceiver feeds our own TX back to RX. The AVR Device
     * delays its reply; discard the echo while the line is still ours. */
    esp_rom_delay_us(1000u);
    uart_flush_input(LINK_UART);
    portENTER_CRITICAL(&rx_lock);
    uart->epoch++;
    uart->tail = uart->head;
    uart->tx_active = false;
    portEXIT_CRITICAL(&rx_lock);
    uart->tx_started = false;
}

#if CONFIG_GUIWIRE_ADAPTIVE_BAUD
static guiwire_status_t uart_set_baud(void *ctx, uint32_t baud) {
    link_uart_t *uart = ctx;
    if (uart_set_baudrate(LINK_UART, baud) != ESP_OK) {
        return GUIWIRE_STATUS_ERR_TRANSPORT;
    }
    uart_flush_input(LINK_UART);
    portENTER_CRITICAL(&rx_lock);
    uart->epoch++;
    uart->tail = uart->head;
    portEXIT_CRITICAL(&rx_lock);
    ESP_LOGI(TAG, "GUIwire baud: %lu", (unsigned long)baud);
    return GUIWIRE_STATUS_OK;
}
#endif

static esp_err_t init_uart(void) {
    uart_config_t config = {
        .baud_rate = LINK_BASE_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    esp_err_t err = uart_driver_install(LINK_UART, 4096u, 2048u, 0u, NULL, 0u);
    if (err != ESP_OK) return err;
    err = uart_param_config(LINK_UART, &config);
    if (err != ESP_OK) return err;
    return uart_set_pin(LINK_UART, GUIWIRE_BOARD_UART_TX,
                        GUIWIRE_BOARD_UART_RX,
                        UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
}

void app_main(void) {
    ESP_ERROR_CHECK(guiwire_board_init());
    guiwire_color_t *pixels = guiwire_board_alloc_framebuffer();
    if (!pixels) {
        ESP_LOGE(TAG, "8 MB PSRAM is required for the 600x450 RGB565 scene");
        return;
    }
    if (!guiwire_framebuffer_init(&framebuffer, pixels,
                                  GUIWIRE_BOARD_WIDTH, GUIWIRE_BOARD_HEIGHT,
                                  guiwire_board_present, NULL)) {
        ESP_LOGE(TAG, "framebuffer setup failed");
        return;
    }
    ESP_ERROR_CHECK(init_uart());
    if (xTaskCreatePinnedToCore(uart_receive_task, "guiwire_rx", 4096u,
                                &link_uart, configMAX_PRIORITIES - 3u,
                                NULL, 1) != pdPASS) {
        ESP_LOGE(TAG, "UART receiver task failed");
        return;
    }

    const guiwire_server_config_t server_cfg = {
        .display_size = {GUIWIRE_BOARD_WIDTH, GUIWIRE_BOARD_HEIGHT},
        .renderer = &guiwire_framebuffer_renderer,
        .renderer_ctx = &framebuffer,
        .server_id = "GUIwire AMOLED Host",
    };
    guiwire_host_io_t io = {
        .write = uart_write_byte,
        .write_ctx = &link_uart,
        .tx_complete = uart_tx_complete,
        .tx_complete_ctx = &link_uart,
        .response_timeout_ms = 500u,
        .discovery_interval_ms = 100u,
        .poll_interval_ms = 10u,
        .session_timeout_ms = 1500u,
        .max_missed_responses = 3u,
        .base_baud = LINK_BASE_BAUD,
        .baud_guard_ms = 10u,
        .baud_fallback_ms = 400u,
        .new_session_token = new_session_token,
        .require_reliable_session = true,
        .read_time_ms = time_ms,
    };
#if CONFIG_GUIWIRE_ADAPTIVE_BAUD
    io.supported_bauds = supported_bauds;
    io.supported_baud_count = sizeof(supported_bauds) / sizeof(supported_bauds[0]);
    io.set_baud = uart_set_baud;
    io.set_baud_ctx = &link_uart;
#endif

    if (guiwire_host_init(&host, &server_cfg, &io) != GUIWIRE_STATUS_OK) {
        ESP_LOGE(TAG, "GUIwire Host init failed");
        return;
    }
    ESP_LOGI(TAG, "ready: UART1 TX=GPIO%d RX=GPIO%d, 9600 baud discovery",
             GUIWIRE_BOARD_UART_TX, GUIWIRE_BOARD_UART_RX);

    bool previous_connected = false;
    uint32_t previous_overflow = 0u;
    uint32_t next_overflow_log_ms = 0u;
    bool display_failure_reported = false;
    for (;;) {
        uint8_t byte;
        unsigned handled = 0u;
        while (handled < 512u && uart_pop_byte(&link_uart, &byte)) {
            guiwire_host_feed_byte(&host, byte, time_ms(NULL));
            ++handled;
        }
        guiwire_host_tick(&host, time_ms(NULL));
        if (host.connected != previous_connected) {
            ESP_LOGI(TAG, "%s at %lu baud", host.connected ? "client connected" : "client disconnected",
                     (unsigned long)host.current_baud);
            previous_connected = host.connected;
        }
        uint32_t now = time_ms(NULL);
        portENTER_CRITICAL(&rx_lock);
        uint32_t overflow = link_uart.overflow;
        portEXIT_CRITICAL(&rx_lock);
        if (overflow != previous_overflow &&
            (int32_t)(now - next_overflow_log_ms) >= 0) {
            previous_overflow = overflow;
            next_overflow_log_ms = now + 1000u;
            ESP_LOGW(TAG, "UART RX ring overflow: %lu bytes", (unsigned long)previous_overflow);
        }
        if (guiwire_server_flush(&host.server) != GUIWIRE_STATUS_OK) {
            if (!display_failure_reported) ESP_LOGE(TAG, "AMOLED update failed");
            display_failure_reported = true;
            vTaskDelay(pdMS_TO_TICKS(20));
        } else {
            display_failure_reported = false;
            vTaskDelay(pdMS_TO_TICKS(1));
        }
    }
}
