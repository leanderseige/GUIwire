# GUIwire RP2040 Host/Server

This project builds the GUIwire Host firmware for:

- ILI9488, 320×480
- SSD1306, 128×32
- SSD1306, 128×64
- ST7789, 240×240

The RP2040 uses UART0 on **GPIO0/TX** and **GPIO1/RX** for the GUIwire
transceiver. These directions are fixed by the RP2040 pin multiplexer:
`gpio_set_function(..., GPIO_FUNC_UART)` cannot turn GPIO1 into UART0 TX or
GPIO0 into UART0 RX.

For the discrete transceiver in the demonstrator, connect GPIO0 to its MCU
`TX` input (R208/Q203 stage) and GPIO1 to its MCU `RX` output (R210/Q204
stage). Keep both transceiver grounds connected. If you wired the RP2040
according to an older revision of this README, **exchange the two signal
leads at the RP2040** before testing the new firmware. The ATmega328P remains
on D1/TX and D0/RX; do not exchange those pins.
The display buses and pin assignments are defined in the respective
`firmware/*/config/board_config.h` files.

## Compile

Set `PICO_SDK_PATH` to a Pico SDK checkout and select the board, for example:
The `build` directory is created by the first CMake command; it is not part of
the repository.

```sh
cmake -S firmware/guiwire_server_rp2040 \
  -B build/guiwire-server -DPICO_BOARD=waveshare_rp2040_zero
cmake --build build/guiwire-server -j2
```

For the Waveshare RP2040 Zero with the ST7789 display, build the dedicated
target with the board profile selected explicitly:

```sh
cmake -S firmware/guiwire_server_rp2040 \
  -B build/guiwire-server-zero-pinfixed \
  -DPICO_BOARD=waveshare_rp2040_zero \
  -DGUIWIRE_UART_BAUD=9600 \
  -DGUIWIRE_ENABLE_BAUD_NEGOTIATION=OFF \
  -DGUIWIRE_SERVER_DEBUG_DISPLAY=ON
cmake --build build/guiwire-server-zero-pinfixed \
  --target guiwire_server_st7789_pico -j2
```

The reference demonstrator is currently deliberately configured as a
failsafe fixed-rate link: both endpoints remain at 9600 baud. Automatic baud
negotiation is disabled until the basic link has been verified. To use another
fixed rate for an electrical test, pass `-DGUIWIRE_UART_BAUD=<rate>` and build
the client with the same `BAUD` value. Do not mix firmware images built with
different fixed rates.

The optional negotiation code can be enabled for development with
`-DGUIWIRE_ENABLE_BAUD_NEGOTIATION=ON` on the RP2040 build and `NEGOTIATE=1`
on the ATmega build. 115200 is not part of the demo capability list.

For a fixed 9600-baud test, for example:

```sh
make -B -C firmware/guiwire_demo_client_atmega328p \
  BAUD=9600 NEGOTIATE=0
```

The resulting ST7789 UF2 is:

```text
build/guiwire-server-zero-pinfixed/firmware/server_st7789_pico/guiwire_server_st7789_pico.uf2
```

The generated UF2 files are:

```text
build/guiwire-server/firmware/server_ili9488_pico/guiwire_server_ili9488_pico.uf2
build/guiwire-server/firmware/server_ssd1306_pico/guiwire_server_ssd1306_128x32_pico.uf2
build/guiwire-server/firmware/server_ssd1306_pico/guiwire_server_ssd1306_128x64_pico.uf2
build/guiwire-server/firmware/server_st7789_pico/guiwire_server_st7789_pico.uf2
```

## Upload with BOOTSEL

1. Hold `BOOTSEL` while connecting or resetting the Pico.
2. Copy the desired UF2 file to the mounted `RPI-RP2` drive.

For the ILI9488 target, on macOS for example:

```sh
cp -X build/guiwire-server/firmware/server_ili9488_pico/guiwire_server_ili9488_pico.uf2 /Volumes/RPI-RP2/
```

Use the matching SSD1306 UF2 path for an OLED Host. The Pico reboots after
the copy completes. The `RPI-RP2` volume can disappear while macOS `cp` is
still waiting for the USB file handle to close. If the Pico has rebooted and
the firmware is running, stop the waiting command with `Ctrl-C`; no second
upload is required.

## Upload with picotool

With the Pico connected and visible to `picotool`:

```sh
picotool load -f build/guiwire-server/firmware/server_ili9488_pico/guiwire_server_ili9488_pico.uf2
picotool reboot
```

Replace the UF2 path with the 128×32 or 128×64 SSD1306 target as required.

For the Waveshare RP2040 Zero ST7789 target, the configured display pins are
SCL/GPIO2, SDA (SPI MOSI)/GPIO3, RES/GPIO4 and DC/GPIO5. CS is disabled because
no CS pin was supplied; the display must keep CS active or the board config
must be changed before compiling. The ZJY-IPS130-V2.0 no-CS variant is
configured for SPI mode 3 at 16 MHz. The target is
`server_st7789_pico/guiwire_server_st7789_pico.uf2`.

The ST7789 server also configures GPIO8 as an active-low debug button: connect
the button between GPIO8 and GND. The internal pull-up is enabled. Pressing
the button shows the diagnostic screen for as long as it remains pressed; it
shows link state, counters and the currently negotiated GUIwire baud rate.
Releasing the button immediately returns to the normal GUI. Keep
`GUIWIRE_SERVER_DEBUG_DISPLAY` enabled when building; it is enabled by
default. In the corrected 9600-baud build the debug heading reads
`GUIWIRE PINFIX 9600`; this verifies that the new RP2040 firmware was loaded.
After a successful connection, `VALID` keeps increasing, `STATE` remains
`CONNECTED` and `CMD` normally settles at `04` (`NO_DATA`) once the initial
scene has arrived. If `VALID` rises only for `CMD 01` (`PING`) and the screen
returns to `Waiting for client...`, check the GPIO0/TX and GPIO1/RX wiring
before changing baud rates or the display code.

Upload the ST7789 build with:

```sh
cp -X build/guiwire-server-zero-pinfixed/firmware/server_st7789_pico/guiwire_server_st7789_pico.uf2 \
  /Volumes/RPI-RP2/
```
