# GUIwire RP2040 Demo Client

This is a GUIwire Device demo. It sends a complete initial interface after
`PING` and periodically updates a progress bar after `POLL` requests.

It starts at 9600 baud and negotiates 38400 or 57600 baud when the Host
advertises and acknowledges that rate. If the new rate does not work, the
Device returns to 9600 baud after its fallback timeout.

## Compile

Set `PICO_SDK_PATH` to a Pico SDK checkout:

```sh
cmake -S firmware/guiwire_demo_client_rp2040 \
  -B build/guiwire-client-rp2040 -DPICO_BOARD=pico
cmake --build build/guiwire-client-rp2040 -j2
```

The firmware is:

```text
build/guiwire-client-rp2040/guiwire_demo_client_rp2040.uf2
```

The default GUIwire transceiver pins are UART0 TX=GPIO0 and RX=GPIO1. These
directions are fixed by the RP2040 UART pin multiplexer. If the hardware uses
other pins, select a valid UART0 pin pair in `config/board_config.h`.

## Upload with BOOTSEL

1. Hold `BOOTSEL` while connecting or resetting the Pico.
2. Copy the UF2 file to the mounted `RPI-RP2` drive:

```sh
cp -X build/guiwire-client-rp2040/guiwire_demo_client_rp2040.uf2 /Volumes/RPI-RP2/
```

The Pico reboots and removes the `RPI-RP2` volume after accepting the UF2.
If macOS leaves `cp` waiting after the reboot, press `Ctrl-C` once the board
has restarted; the upload is already complete.

## Upload with picotool

```sh
picotool load -f build/guiwire-client-rp2040/guiwire_demo_client_rp2040.uf2
picotool reboot
```
