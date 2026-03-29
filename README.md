# GUI2C

A compact binary GUI protocol for small microcontrollers.

GUI2C is split into:
- a transport-agnostic protocol and object model
- client helpers for encoding commands
- server-side object storage and rendering
- renderer/display drivers
- platform adapters

The current repository includes a Pico-based ILI9488 server and a Pico demo client. The server receives GUI packets over I2C and renders them synchronously to the display over SPI.

## Current scope

This repository currently provides:

- a compact binary GUI2C protocol
- client-managed object IDs
- server-reserved object ID `0` for the boot/status label
- text objects with create/update/destroy support
- rectangle objects with create/update/destroy support
- `DESTROY_ALL_OBJECTS` as a clear-screen-style command
- Pico platform adapters for I2C, SPI, GPIO and delay
- interrupt-driven I2C packet reception on the Pico ILI9488 server
- ILI9488 server firmware
- Pico client firmware demo

## Object model

Implemented commands:

- `PING`
- `HELLO_STRING`
- `DESTROY_ALL_OBJECTS`
- `CREATE_TEXT`
- `UPDATE_TEXT`
- `DESTROY_OBJECT`
- `CREATE_RECT`
- `UPDATE_RECT`

Rules:

- object id `0` is reserved for the server
- client objects should use ids `>= 1`
- `DESTROY_ALL_OBJECTS` removes only client-owned objects
- commands are processed in reception order
- rendering is synchronous: one command is executed, rendered and flushed before the next command is handled

Current object types:

### Text object

Fields:

- `object_id`
- `x`, `y`, `w`, `h`
- `fg`, `bg`
- `flags`
- `zoom`
- `text`

Notes:

- text background can be opaque or transparent
- text updates currently redraw only changed character cells when possible

### Rect object

Fields:

- `object_id`
- `x`, `y`, `w`, `h`
- `fill`
- `border`
- `flags`

Notes:

- fill can be opaque or transparent
- border is optional
- border is always 1 pixel and drawn inside the rectangle bounds

## Build

Set the environment variable to point to your Pico SDK.

```bash
export PICO_SDK_PATH=$HOME/pico-sdk
```

Compile server and client code.

```bash
cd gui2c
mkdir build
cd build
cmake ..
make -j4
find . -name "*.uf2"
```

## Firmware targets

- `gui2c_server_ili9488_pico`
- `gui2c_client_object_demo_pico`

## Repository layout

- `include/gui2c/`
  - public protocol, client, server and type definitions
- `lib/core/`
  - protocol encode/decode and shared helpers
- `lib/client/`
  - command serialization helpers
- `lib/server/`
  - server state and object storage
- `lib/objects/`
  - per-object-type create/update/draw/destroy logic
- `lib/renderer/`
  - renderer helper code such as glyph rendering
- `lib/display/`
  - concrete display drivers such as ILI9488
- `platform/`
  - MCU/platform glue
- `firmware/`
  - buildable firmware examples
- `docs/`
  - focused reference and how-to documentation

## Documentation

The `docs/` directory contains four focused documents:

- `docs/protocol.md`
  - packet structure, command payloads, flags, limits and rendering semantics
- `docs/porting.md`
  - architectural layering, platform responsibilities, porting order and common pitfalls
- `docs/howto_add_object_type.md`
  - step-by-step guide for introducing a new GUI object type
- `docs/howto_write_client.md`
  - step-by-step guide for implementing a custom GUI2C client

## Status and limitations

Current implementation notes:

- rendering is synchronous
- there is no generic z-order system yet
- object types are currently stored in separate arrays
- overlapping transparent objects are still a simple model, not a full compositor
- large filled areas depend heavily on display driver throughput and SPI speed
