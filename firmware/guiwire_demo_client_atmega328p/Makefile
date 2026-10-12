# SPDX-License-Identifier: MIT

MCU ?= atmega328p
F_CPU ?= 16000000UL
BAUD ?= 9600
NEGOTIATE ?= 0
UPLOAD_BAUD ?= 57600
PROGRAMMER ?= arduino
PORT ?= /dev/ttyUSB0

CC := avr-gcc
OBJCOPY := avr-objcopy
SIZE := avr-size

COMMON_DIR := ../common
BUILD_DIR := build
TARGET := $(BUILD_DIR)/guiwire_demo_client_atmega328p
SOURCES := main.c \
	$(COMMON_DIR)/src/guiwire_color.c \
	$(COMMON_DIR)/src/guiwire_device.c \
	$(COMMON_DIR)/src/guiwire_device_runtime.c \
	$(COMMON_DIR)/src/guiwire_link.c \
	$(COMMON_DIR)/src/guiwire_protocol.c \
	$(COMMON_DIR)/src/guiwire_status.c

CFLAGS := -mmcu=$(MCU) -DF_CPU=$(F_CPU) -DBAUD=$(BAUD) -DGUIWIRE_ENABLE_BAUD_NEGOTIATION=$(NEGOTIATE) \
	-DGUIWIRE_DEVICE_QUEUE_CAPACITY=9u -DGUIWIRE_MAX_PACKET_SIZE=98u \
	-std=c11 -Os -Wall -Wextra \
	-I$(COMMON_DIR)/include -I$(COMMON_DIR)/src
LDFLAGS := -mmcu=$(MCU)

.PHONY: all clean flash size

all: $(TARGET).hex

$(BUILD_DIR):
	mkdir -p $@

$(TARGET).elf: $(SOURCES) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(SOURCES) $(LDFLAGS) -o $@

$(TARGET).hex: $(TARGET).elf
	$(OBJCOPY) -O ihex -R .eeprom $< $@

size: $(TARGET).elf
	$(SIZE) $<

flash: $(TARGET).hex
	avrdude -p $(MCU) -c $(PROGRAMMER) -P $(PORT) -b $(UPLOAD_BAUD) -U flash:w:$<:i

clean:
	rm -rf $(BUILD_DIR)
