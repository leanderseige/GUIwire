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
- line objects with create/update/destroy support
- bar objects with create/update/destroy support
- ring objects with create/update/destroy support
- waveform objects with create/update/destroy support
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
- `CREATE_LINE`
- `UPDATE_LINE`
- `CREATE_BAR`
- `UPDATE_BAR`
- `CREATE_RING`
- `UPDATE_RING`
- `CREATE_WAVEFORM`
- `UPDATE_WAVEFORM`

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
- geometry is defined at create time
- updates change text/style only and currently redraw only changed character cells when possible

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
- geometry is defined at create time
- updates change fill/border/flags only

### Line object

Fields:

- `object_id`
- `x1`, `y1`, `x2`, `y2`
- `fg`, `bg`
- `thickness`

Notes:

- geometry is defined at create time
- updates change colors only
- `thickness = 0` is treated as `1`

### Bar object

Fields:

- `object_id`
- `x`, `y`, `w`, `h`
- `fg`, `bg`
- `value`
- `direction`

Notes:

- `value` is a `u16` fill level from empty to full
- supported directions are top-to-bottom, bottom-to-top, left-to-right and right-to-left
- updates usually only change `value`
- on the current server, updates redraw only the changed fill delta instead of rebuilding the whole widget

### Ring object

Fields:

- `object_id`
- `cx`, `cy`
- `radius`, `inner_radius`
- `opening_angle_deg`
- `bg`, `fg_active`, `fg_inactive`
- `value`

Notes:

- renders a filled ring or disk depending on `inner_radius`
- filling starts at the lower apex and proceeds clockwise
- a non-zero `opening_angle_deg` leaves a symmetric gap at the bottom and makes the fill start at the left end of that gap
- updates usually only change `value`
- on the current server, value-only updates redraw only the ring sectors whose active/inactive state changed

### Waveform object

Fields:

- `object_id`
- `x`, `y`, `w`, `h`
- `border`, `bg`
- `flags`
- `wave_count`
- `wave_colors[]`

Notes:

- one widget can display up to `GUI2C_MAX_WAVEFORM_WAVES` curves
- updates address one `wave_index` at a time
- full replacement stores a complete value set for one wave
- shifting can update one wave or multiple waves in one packet
- shifting stores time history right-to-left and redraws the widget from the stored raw data
- autoscaling uses the current maximum over all stored waveform values in the widget
- server-side waveform storage limits are hardware-profile dependent, not part of the public client protocol
- the current ILI9488-oriented server path renders the plot area column-by-column to avoid full-widget blanking and visible flicker

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
- `gui2c_client_brain_module_pico`

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
- `CREATE_*` commands are idempotent on the server and are safe to resend for resync purposes
- object geometry is currently immutable after `CREATE_*`
- server-side optimization and memory limits can depend on the selected server target profile
- object updates may use target-specific partial redraw strategies as long as the public object semantics stay unchanged
- overlapping transparent objects are still a simple model, not a full compositor
- large filled areas depend heavily on display driver throughput and SPI speed
