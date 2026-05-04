<!--
SPDX-License-Identifier: MIT
Authors: any:frequency, seige.digital GbR, Leander Seige (2026)
-->

# GUI2C Architecture

## Repositories

GUI2C is split into four repositories:

- `gui2c`: protocol specification and documentation
- `gui2c_client_library_rp2350_rp2040`: Pico-SDK C library for GUI2C clients
- `gui2c_server_ili9488`: ILI9488 display server firmware and renderer stack
- `gui2c_demo_client`: example Pico client firmware using the client library

The specification repository is the source of truth for the wire protocol. C
headers in the implementation repositories should remain derived from this
specification.

## Roles

GUI2C uses these application-level roles:

- Display server: owns the physical display and renders GUI objects.
- Client: owns the content and sends drawing/object commands.

On the current I2C transport, the bus roles are:

- Client is the I2C controller.
- Display server is the I2C target.

This avoids multi-controller I2C on a Eurorack bus and keeps the first hardware
version predictable.

## Single-View Mode

The first implementation targets one client and one display server:

```text
Eurorack module MCU  ->  GUI2C display MCU  ->  ILI9488
GUI2C client             GUI2C server
I2C controller           I2C target
```

The client normally owns the full advertised display area.

## Multi-View Roadmap

A future multi-view display should avoid multiple controllers on one I2C bus.
The preferred hardware direction is multiple independent I2C ports:

```text
Client A / controller  ->  Port A  \
Client B / controller  ->  Port B   -> Display MCU -> physical display
Client C / controller  ->  Port C  /
```

Each port remains a normal one-controller I2C bus. The display server maps each
physical port to one or more virtual views.

Jumper or DIP-switch configuration can select modes such as:

- single full-screen view
- dual horizontal split
- dual vertical split
- quad split
- diagnostic/test mode
- I2C address, rotation or brightness defaults

Clients should not need to know the global physical placement. They query the
server capabilities on their link and draw into the local coordinate space of
their assigned view.

## Protocol Roadmap

The current command set is object-oriented and immediate. The next protocol
revision should add:

- capability discovery
- virtual display/view descriptors
- view reservation
- session IDs
- timeout and release semantics
- explicit reset behavior
- optional present/update batching

These features should be specified in `docs/protocol.md` before implementation
headers are changed.
