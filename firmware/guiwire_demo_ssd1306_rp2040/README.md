# GUIwire SSD1306 RP2040 Display Demo

This is a standalone display smoke test for a 128×32 SSD1306 module. It does
not need a second GUIwire module: it initializes the OLED over local SPI and
draws a moving frame, progress bar and marker.

## Compile

Set `PICO_SDK_PATH` to a Pico SDK checkout:

```sh
cmake -S firmware/guiwire_demo_ssd1306_rp2040 \
  -B build/guiwire-ssd1306-demo -DPICO_BOARD=pico
cmake --build build/guiwire-ssd1306-demo -j2
```

The firmware is:

```text
build/guiwire-ssd1306-demo/guiwire_demo_ssd1306_rp2040.uf2
```

The default SPI pins are MOSI=GPIO3, SCK=GPIO2, CS=GPIO5, DC=GPIO6 and
RESET=GPIO7. Adjust `config/board_config.h` for the actual module wiring.

## Upload with BOOTSEL

1. Hold `BOOTSEL` while connecting or resetting the Pico.
2. Copy the UF2 file to the mounted `RPI-RP2` drive:

```sh
cp -X build/guiwire-ssd1306-demo/guiwire_demo_ssd1306_rp2040.uf2 /Volumes/RPI-RP2/
```

The Pico reboots and removes the `RPI-RP2` volume after accepting the UF2.
If macOS leaves `cp` waiting after the reboot, press `Ctrl-C` once the board
has restarted; the upload is already complete.

## Upload with picotool

```sh
picotool load -f build/guiwire-ssd1306-demo/guiwire_demo_ssd1306_rp2040.uf2
picotool reboot
```
