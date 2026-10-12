/*
 * SPDX-License-Identifier: MIT
 *
 * GUIwire Demonstrator Device for the ATmega328P.
 *
 * The displayed time is elapsed time since boot. The lower objects show ADC0,
 * ADC1 and hundredths of a second as filled pie charts. The ADC charts show
 * the potentiometer voltage as 0..100 percent; the time chart shows the
 * current second from 0..99 hundredths.
 */

#include <stdint.h>

#include <avr/interrupt.h>
#include <avr/io.h>
#include <util/delay.h>

#include "guiwire/guiwire_device_runtime.h"

#define OBJECT_TIME_TEXT  1u
#define OBJECT_ADC0_RING  2u
#define OBJECT_ADC1_RING  3u
#define OBJECT_100THS_RING 4u
#define OBJECT_ADC0_TEXT  5u
#define OBJECT_ADC1_TEXT  6u
#define OBJECT_100THS_TEXT 7u

#define COLOR_BLACK       0x0000u
#define COLOR_WHITE       0xFFFFu
#define COLOR_DARK_GREY   0x4208u
#define UART_RX_RING_SIZE 64u

#ifndef GUIWIRE_ENABLE_BAUD_NEGOTIATION
#define GUIWIRE_ENABLE_BAUD_NEGOTIATION 0
#endif

static volatile uint32_t g_millis;
static guiwire_device_runtime_t g_runtime;
static uint32_t g_now_ms;
static uint32_t g_next_update_ms;
static uint8_t g_last_hundredths;
static guiwire_color_t g_adc0_color;
static guiwire_color_t g_adc1_color;
static guiwire_color_t g_hundredths_color;
static uint32_t g_random_state = 0x7A3D21C5UL;
static char g_time_text[9];
/* "A0: 100%" and "A1: 100%" need 8 visible characters plus NUL. */
static char g_adc0_text[9];
static char g_adc1_text[9];
/* "1/100: 100%" needs 11 visible characters plus NUL. */
static char g_hundredths_text[12];
static volatile uint8_t g_uart_rx_ring[UART_RX_RING_SIZE];
static volatile uint8_t g_uart_rx_head;
static volatile uint8_t g_uart_rx_tail;
static volatile uint8_t g_uart_rx_fault;
static uint16_t g_adc_sample[2];
static uint8_t g_adc_channel;
static uint8_t g_adc_discard;

static const guiwire_color_t g_palette[] = {
    0xF800u, /* red */
    0xFD20u, /* orange */
    0xFFE0u, /* yellow */
    0x07E0u, /* green */
    0x07FFu, /* cyan */
    0x001Fu, /* blue */
    0x801Fu, /* violet */
    0xF81Fu  /* magenta */
};

#if GUIWIRE_ENABLE_BAUD_NEGOTIATION
static const uint32_t gui_wire_supported_bauds[] = {
    9600UL, 19200UL, 38400UL, 57600UL
};
#endif

static void uart_init_baud(uint32_t baud) {
    uint16_t ubrr = (uint16_t)((F_CPU / (8UL * baud)) - 1UL);
    UCSR0A = (uint8_t)(1u << U2X0);
    UBRR0H = (uint8_t)(ubrr >> 8);
    UBRR0L = (uint8_t)ubrr;
    UCSR0B = (uint8_t)((1u << RXEN0) | (1u << TXEN0) | (1u << RXCIE0));
    UCSR0C = (uint8_t)((1u << UCSZ01) | (1u << UCSZ00));
}

ISR(USART_RX_vect) {
    uint8_t status = UCSR0A;
    uint8_t byte = UDR0;
    uint8_t next = (uint8_t)((g_uart_rx_head + 1u) & (UART_RX_RING_SIZE - 1u));
    if ((status & ((1u << FE0) | (1u << DOR0) | (1u << UPE0))) != 0u ||
        next == g_uart_rx_tail) {
        g_uart_rx_fault = 1u;
        return;
    }
    g_uart_rx_ring[g_uart_rx_head] = byte;
    g_uart_rx_head = next;
}

static uint8_t uart_rx_pop(uint8_t *byte) {
    uint8_t tail = g_uart_rx_tail;
    if (tail == g_uart_rx_head) return 0u;
    *byte = g_uart_rx_ring[tail];
    g_uart_rx_tail = (uint8_t)((tail + 1u) & (UART_RX_RING_SIZE - 1u));
    return 1u;
}

static uint8_t uart_rx_take_fault(void) {
    uint8_t saved_sreg = SREG;
    uint8_t fault;
    cli();
    fault = g_uart_rx_fault;
    g_uart_rx_fault = 0u;
    if (fault) g_uart_rx_tail = g_uart_rx_head;
    SREG = saved_sreg;
    return fault;
}

#if GUIWIRE_ENABLE_BAUD_NEGOTIATION
static guiwire_status_t uart_set_baud(void *ctx, uint32_t baud) {
    uint32_t ubrr_value;
    (void)ctx;
    if (!baud) return GUIWIRE_STATUS_ERR_INVALID_ARGUMENT;
    ubrr_value = (F_CPU / (8UL * baud)) - 1UL;
    if (ubrr_value > 65535UL) return GUIWIRE_STATUS_ERR_INVALID_ARGUMENT;
    /* tx_complete has already waited for the final stop bit and CLEARED
     * TXC0. Waiting for TXC0 here would deadlock until another transmission. */
    UCSR0B &= (uint8_t)~((1u << RXEN0) | (1u << RXCIE0));
    g_uart_rx_tail = g_uart_rx_head;
    UBRR0H = (uint8_t)(ubrr_value >> 8);
    UBRR0L = (uint8_t)ubrr_value;
    UCSR0B |= (uint8_t)((1u << RXEN0) | (1u << RXCIE0));
    return GUIWIRE_STATUS_OK;
}
#endif

static guiwire_status_t uart_write_byte(void *ctx, uint8_t byte) {
    (void)ctx;
    while ((UCSR0A & (1u << UDRE0)) == 0u) { }
    UDR0 = byte;
    return GUIWIRE_STATUS_OK;
}

static void uart_tx_complete(void *ctx) {
    (void)ctx;
    while ((UCSR0A & (1u << TXC0)) == 0u) { }
    UCSR0A |= (uint8_t)(1u << TXC0);
    _delay_ms(1.0);
    /* The transceiver is half-duplex. Re-enable RX only after our final bit
     * has left the wire, so our own response cannot enter the parser. */
    UCSR0B |= (uint8_t)((1u << RXEN0) | (1u << RXCIE0));
}

static void uart_tx_begin(void *ctx) {
    (void)ctx;
    /* Suppress the local transceiver echo for the complete response. */
    UCSR0B &= (uint8_t)~((1u << RXEN0) | (1u << RXCIE0));
    _delay_ms(2.0);
}

static void adc_init(void) {
    /* AVcc (the Nano's 5 V rail) is the ADC reference. */
    ADMUX = (uint8_t)(1u << REFS0);
    ADCSRA = (uint8_t)((1u << ADEN) | (1u << ADPS2) |
                       (1u << ADPS1) | (1u << ADPS0));
    DIDR0 = (uint8_t)((1u << ADC0D) | (1u << ADC1D));
    g_adc_channel = 0u;
    g_adc_discard = 1u;
    ADCSRA |= (uint8_t)(1u << ADSC);
}

static void adc_service(void) {
    if ((ADCSRA & (1u << ADSC)) != 0u) return;
    if (g_adc_discard) {
        g_adc_discard = 0u;
    } else {
        g_adc_sample[g_adc_channel] = ADC;
        g_adc_channel ^= 1u;
        ADMUX = (uint8_t)((1u << REFS0) | g_adc_channel);
        g_adc_discard = 1u;
    }
    ADCSRA |= (uint8_t)(1u << ADSC);
}

static uint32_t random_next(void) {
    g_random_state = g_random_state * 1664525UL + 1013904223UL;
    return g_random_state;
}

static guiwire_color_t random_new_color(guiwire_color_t previous) {
    guiwire_color_t color;

    do {
        color = g_palette[random_next() % (sizeof(g_palette) / sizeof(g_palette[0]))];
    } while (color == previous);
    return color;
}

static uint16_t pie_from_percent(uint8_t percent) {
    return (uint16_t)(((uint32_t)percent * 65535UL) / 100UL);
}

static void format_time(uint32_t elapsed_ms) {
    uint32_t total_seconds = elapsed_ms / 1000UL;
    uint8_t minutes = (uint8_t)((total_seconds / 60UL) % 100UL);
    uint8_t seconds = (uint8_t)(total_seconds % 60UL);
    uint8_t hundredths = (uint8_t)((elapsed_ms / 10UL) % 100UL);

    g_time_text[0] = (char)('0' + minutes / 10u);
    g_time_text[1] = (char)('0' + minutes % 10u);
    g_time_text[2] = ':';
    g_time_text[3] = (char)('0' + seconds / 10u);
    g_time_text[4] = (char)('0' + seconds % 10u);
    g_time_text[5] = '.';
    g_time_text[6] = (char)('0' + hundredths / 10u);
    g_time_text[7] = (char)('0' + hundredths % 10u);
    g_time_text[8] = '\0';
}

static void format_percent(char *dst, const char *label, uint8_t label_len,
                           uint8_t percent) {
    uint8_t pos = 0u;
    uint8_t divisor;

    while (pos < label_len) dst[pos] = label[pos], ++pos;
    dst[pos++] = ':';
    dst[pos++] = ' ';
    if (percent >= 100u) {
        dst[pos++] = '1';
        dst[pos++] = '0';
        dst[pos++] = '0';
    } else {
        divisor = 10u;
        if (percent >= 10u) dst[pos++] = (char)('0' + percent / divisor);
        dst[pos++] = (char)('0' + percent % divisor);
    }
    dst[pos++] = '%';
    dst[pos] = '\0';
}

static uint8_t adc_to_percent(uint16_t sample) {
    return (uint8_t)(((uint32_t)sample * 100UL + 511UL) / 1023UL);
}

static uint32_t millis_now(void) {
    uint8_t saved_sreg = SREG;
    uint32_t result;
    cli();
    result = g_millis;
    SREG = saved_sreg;
    return result;
}

static void queue_snapshot_text(guiwire_device_t *device,
                                guiwire_object_id_t object_id,
                                uint16_t x, uint16_t y, uint16_t w,
                                uint8_t zoom, const char *text) {
    guiwire_text_desc_t desc = {
        .object_id = object_id,
        .x = x, .y = y, .w = w,
        .h = zoom == 4u ? 36u : 10u,
        .fg = COLOR_WHITE, .bg = COLOR_BLACK,
        .flags = 0u, .zoom = zoom, .text = text
    };
    (void)guiwire_device_create_text(device, &desc);
}

static void queue_snapshot_ring(guiwire_device_t *device,
                                guiwire_object_id_t object_id, uint16_t cx,
                                uint8_t percent, guiwire_color_t color) {
    guiwire_ring_desc_t desc = {
        .object_id = object_id,
        .cx = cx, .cy = 178u, .radius = 30u, .inner_radius = 0u,
        .opening_angle_deg = 0u, .bg = COLOR_BLACK,
        .fg_active = color, .fg_inactive = COLOR_DARK_GREY,
        .value = pie_from_percent(percent)
    };
    (void)guiwire_device_create_ring(device, &desc);
}

static void build_snapshot(void *ctx, guiwire_device_t *device) {
    uint32_t now_ms;
    uint8_t adc0_percent;
    uint8_t adc1_percent;
    uint8_t hundredths;

    (void)ctx;
    g_adc0_color = random_new_color(0u);
    g_adc1_color = random_new_color(g_adc0_color);
    g_hundredths_color = random_new_color(g_adc1_color);
    now_ms = millis_now();
    hundredths = (uint8_t)((now_ms / 10UL) % 100UL);
    g_last_hundredths = hundredths;
    g_next_update_ms = 0u;

    adc0_percent = adc_to_percent(g_adc_sample[0]);
    adc1_percent = adc_to_percent(g_adc_sample[1]);
    format_time(now_ms);
    format_percent(g_adc0_text, "A0", 2u, adc0_percent);
    format_percent(g_adc1_text, "A1", 2u, adc1_percent);
    format_percent(g_hundredths_text, "1/100", 5u, hundredths);

    (void)guiwire_device_destroy_all_objects(device);
    (void)guiwire_device_queue_hello(device, "GUIwire ATmega328P clock demo");
    queue_snapshot_text(device, OBJECT_TIME_TEXT, 16u, 24u, 208u, 4u, g_time_text);
    queue_snapshot_ring(device, OBJECT_ADC0_RING, 40u, adc0_percent, g_adc0_color);
    queue_snapshot_ring(device, OBJECT_ADC1_RING, 120u, adc1_percent, g_adc1_color);
    queue_snapshot_ring(device, OBJECT_100THS_RING, 200u, hundredths, g_hundredths_color);
    queue_snapshot_text(device, OBJECT_ADC0_TEXT, 8u, 212u, 64u, 1u, g_adc0_text);
    queue_snapshot_text(device, OBJECT_ADC1_TEXT, 88u, 212u, 64u, 1u, g_adc1_text);
    queue_snapshot_text(device, OBJECT_100THS_TEXT, 168u, 212u, 64u, 1u, g_hundredths_text);
}

static void timer_init(void) {
    TCCR0A = (uint8_t)(1u << WGM01);
    TCCR0B = (uint8_t)((1u << CS01) | (1u << CS00));
    OCR0A = 249u;
    TIMSK0 = (uint8_t)(1u << OCIE0A);
}

ISR(TIMER0_COMPA_vect) {
    ++g_millis;
}

static void update_display(guiwire_device_t *device, uint32_t elapsed_ms) {
    uint8_t adc0_percent = adc_to_percent(g_adc_sample[0]);
    uint8_t adc1_percent = adc_to_percent(g_adc_sample[1]);
    uint8_t hundredths = (uint8_t)((elapsed_ms / 10UL) % 100UL);
    uint8_t hundredths_wrapped = (uint8_t)(hundredths < g_last_hundredths);
    guiwire_text_update_t text_update;
    guiwire_ring_update_t ring_update;

    if (hundredths_wrapped) {
        g_hundredths_color = random_new_color(g_hundredths_color);
    }

    format_time(elapsed_ms);
    text_update.object_id = OBJECT_TIME_TEXT;
    text_update.fg = COLOR_WHITE;
    text_update.bg = COLOR_BLACK;
    text_update.flags = 0u;
    text_update.zoom = 4u;
    text_update.text = g_time_text;
    (void)guiwire_device_update_text(device, &text_update);

    ring_update.object_id = OBJECT_ADC0_RING;
    ring_update.value = pie_from_percent(adc0_percent);
    (void)guiwire_device_update_ring(device, &ring_update);
    ring_update.object_id = OBJECT_ADC1_RING;
    ring_update.value = pie_from_percent(adc1_percent);
    (void)guiwire_device_update_ring(device, &ring_update);

    /* Ring colours are part of the create packet. Recreate the time ring only
     * when the second wraps, so the colour change needs no new protocol field. */
    if (hundredths_wrapped) {
        const guiwire_ring_desc_t desc = {
            .object_id = OBJECT_100THS_RING, .cx = 200u, .cy = 178u,
            .radius = 30u, .inner_radius = 0u, .opening_angle_deg = 0u,
            .bg = COLOR_BLACK, .fg_active = g_hundredths_color,
            .fg_inactive = COLOR_DARK_GREY,
            .value = pie_from_percent(hundredths)
        };
        (void)guiwire_device_destroy_object(device, OBJECT_100THS_RING);
        (void)guiwire_device_create_ring(device, &desc);
    } else {
        ring_update.object_id = OBJECT_100THS_RING;
        ring_update.value = pie_from_percent(hundredths);
        (void)guiwire_device_update_ring(device, &ring_update);
    }

    text_update.object_id = OBJECT_ADC0_TEXT;
    text_update.zoom = 1u;
    format_percent(g_adc0_text, "A0", 2u, adc0_percent);
    text_update.text = g_adc0_text;
    (void)guiwire_device_update_text(device, &text_update);

    text_update.object_id = OBJECT_ADC1_TEXT;
    format_percent(g_adc1_text, "A1", 2u, adc1_percent);
    text_update.text = g_adc1_text;
    (void)guiwire_device_update_text(device, &text_update);

    text_update.object_id = OBJECT_100THS_TEXT;
    format_percent(g_hundredths_text, "1/100", 5u, hundredths);
    text_update.text = g_hundredths_text;
    (void)guiwire_device_update_text(device, &text_update);

    g_last_hundredths = hundredths;
}

/* Keep the large IO configuration local out of main's permanent stack frame. */
__attribute__((noinline))
static guiwire_status_t initialize_runtime(void) {
    guiwire_device_io_t io = {0};

    io.write = uart_write_byte;
    io.write_ctx = NULL;
    io.tx_begin = uart_tx_begin;
    io.tx_begin_ctx = NULL;
    io.tx_complete = uart_tx_complete;
    io.tx_complete_ctx = NULL;
#if GUIWIRE_ENABLE_BAUD_NEGOTIATION
    /* Survive a failed high-rate trial and the Host's quiet fallback window
     * without discarding the GUI queue. The Host still detects unplugging. */
    io.session_timeout_ms = 2500u;
#else
    /* Keep the known-good fixed-rate session behavior unchanged. */
    io.session_timeout_ms = 1500u;
#endif
    io.echo_guard_ms = 5u;
    io.base_baud = BAUD;
#if GUIWIRE_ENABLE_BAUD_NEGOTIATION
    io.supported_bauds = gui_wire_supported_bauds;
    io.supported_baud_count = (uint8_t)(sizeof(gui_wire_supported_bauds) /
                                        sizeof(gui_wire_supported_bauds[0]));
    io.baud_fallback_ms = 400u;
    io.set_baud = uart_set_baud;
    io.set_baud_ctx = NULL;
#else
    io.supported_bauds = NULL;
    io.supported_baud_count = 0u;
    io.baud_fallback_ms = 0u;
    io.set_baud = NULL;
    io.set_baud_ctx = NULL;
#endif
    return guiwire_device_runtime_init(&g_runtime, &io, build_snapshot, NULL, NULL);
}

int main(void) {
    uint8_t byte;
    uart_init_baud(BAUD);
    adc_init();
    timer_init();
    if (initialize_runtime() != GUIWIRE_STATUS_OK) return 1;
    sei();

    for (;;) {
        g_now_ms = millis_now();
        if (uart_rx_take_fault()) {
            guiwire_link_reset(&g_runtime.link);
        }
        while (uart_rx_pop(&byte)) {
            guiwire_device_runtime_feed_byte(&g_runtime, byte, g_now_ms);
        }
        guiwire_device_runtime_tick(&g_runtime, g_now_ms);
        adc_service();
        if (guiwire_device_runtime_connected(&g_runtime) &&
            g_now_ms >= g_next_update_ms &&
            guiwire_device_pending(&g_runtime.device) == 0u) {
            update_display(&g_runtime.device, g_now_ms);
            g_next_update_ms = g_now_ms + 50u;
        }
    }
}
