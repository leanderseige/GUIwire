<!--
SPDX-License-Identifier: MIT
Authors: any:frequency, seige.digital GbR, Leander Seige (2026)
-->

# GUIwire

GUIwire is a bidirectional, patchable interface for display and control
modules in the Eurorack format.

Its authoritative specification is [`GUIwire.md`](GUIwire.md). The companion
documents provide the packet registry and the implementation architecture:

- [`docs/protocol.md`](docs/protocol.md): normative packet and object details
- [`docs/architecture.md`](docs/architecture.md): roles, link operation and
  implementation boundaries

## Protocol model

GUIwire separates a functional **Device** from a **Host** that provides the
user interface. The Device owns the functional state. The Host renders the
current interface and may eventually return interaction events.

The current application profile is a compact binary object protocol. It
retains the established object semantics for text, rectangles, lines, bars,
rings and waveforms. All multi-byte values are little-endian and colours use
RGB565.

The transport is independent of the display technology:

```text
Device ── 3.5 mm TS patch cable ── Host
         DATA + GND, half duplex
```

No supply voltage is carried by the cable. Each module powers its own
transceiver and display electronics.

## Current scope

The protocol defines:

- a protected single-wire physical connection
- framed byte transport with synchronization and CRC-16
- safe startup at 9600 baud with optional negotiated higher rates
- host/device session and turnaround rules
- deterministic hot-patch handover and display resynchronization
- `PING` and `HELLO_STRING`
- creation, update and destruction of GUI objects
- bounded packet, text, object and waveform parameters

The primary topology is one Host connected to one Device. Multidrop operation,
line arbitration and general-purpose Eurorack networking are out of scope.

## Portable C library

`firmware/common` is the portable reference library. It contains the complete
wire protocol, framing and CRC layer, the Host scheduler and scene renderer,
the Device queue/object API, and the session/hot-patch state machines. It does
not include MCU headers, an RTOS, dynamic allocation or a specific display
controller.

Manufacturers provide only a small port adapter:

- byte transmission for the shared link
- a TX-complete/turnaround hook
- a renderer with `fill_rect`, `draw_pixel` and `flush`
- the module's functional application code

The public entry point is [`guiwire.h`](firmware/common/include/guiwire/guiwire.h).
The runtime helpers are [`guiwire_host.h`](firmware/common/include/guiwire/guiwire_host.h)
and [`guiwire_device_runtime.h`](firmware/common/include/guiwire/guiwire_device_runtime.h).
Both runtimes use caller-owned storage and compile-time limits, so they are
usable on small MCUs such as the ATmega328P.

The library can be consumed directly with CMake:

```sh
cmake -S firmware/common -B build/guiwire \
  -DCMAKE_INSTALL_PREFIX="$PWD/build/guiwire-install"
cmake --build build/guiwire
cmake --install build/guiwire
```

`firmware/ports/pico` is only the reference RP2040 transport port. The display
drivers under `firmware/ports/pico/displays` are examples of hardware-specific
renderer adapters and are not required by the portable core.

## Reference firmware in this repository

The firmware uses the RP2040 UART as the local interface to the GUIwire
transceiver; the display bus remains local to the Host module.

- `firmware/guiwire_server_rp2040`: Host/server firmware for an ILI9488 and
  SSD1306 displays, plus a 240×240 ST7789 target for the Waveshare RP2040
  Zero. The SSD1306 project emits both `128x32` and `128x64` firmware targets.
- `firmware/guiwire_demo_client_rp2040`: RP2040 Device demo client.
- `firmware/guiwire_demo_ssd1306_rp2040`: standalone 128×32 SSD1306 SPI
  display smoke-test firmware for checking a module without a second GUIwire
  participant.
- `firmware/guiwire_demo_client_atmega328p`: AVR-GCC Device demo client with
  a small four-packet initial scene for the ATmega328P SRAM budget.

Build- und Upload-Anleitungen stehen direkt bei den Projekten:

- [`firmware/guiwire_server_rp2040/README.md`](firmware/guiwire_server_rp2040/README.md)
- [`firmware/guiwire_demo_client_rp2040/README.md`](firmware/guiwire_demo_client_rp2040/README.md)
- [`firmware/guiwire_demo_ssd1306_rp2040/README.md`](firmware/guiwire_demo_ssd1306_rp2040/README.md)
- [`firmware/guiwire_demo_client_atmega328p/README.md`](firmware/guiwire_demo_client_atmega328p/README.md)

The Pico projects expect `PICO_SDK_PATH` to point to a Pico SDK checkout. For
example:

```sh
cmake -S firmware/guiwire_server_rp2040 -B build/server -DPICO_BOARD=pico
cmake --build build/server

cmake -S firmware/guiwire_demo_client_rp2040 -B build/client -DPICO_BOARD=pico
cmake --build build/client

cmake -S firmware/guiwire_demo_ssd1306_rp2040 -B build/oled-demo -DPICO_BOARD=pico
cmake --build build/oled-demo

make -C firmware/guiwire_demo_client_atmega328p
```

Additional manufacturer ports may live in separate repositories. This
repository contains the portable reference library and remains the source of
truth for the interoperable GUIwire specification.
