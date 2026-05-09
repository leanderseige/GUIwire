<!--
SPDX-License-Identifier: MIT
Authors: any:frequency, seige.digital GbR, Leander Seige (2026)
-->

# GUI2C (proof of concept)

GUI2C is a compact binary protocol for driving small display modules from
microcontroller-based Eurorack modules.

This repository is now the protocol/specification repository. Firmware and C
implementation code live in separate repos:

- `../gui2c_client_library_rp2350_rp2040`: Pico-SDK C client library
- `../gui2c_server_ili9488`: Pico-SDK ILI9488 display-server firmware
- `../gui2c_demo_client`: Pico-SDK demo client firmware

## Protocol Model

GUI2C defines an application-level client/server relationship:

- a GUI2C display server offers display services
- a GUI2C client reserves and updates a display view
- current packets are binary, little-endian command frames
- current object IDs are client-managed, with object ID `0` reserved by the server

For the first I2C implementation, the electrical/bus roles are:

- GUI2C client: I2C controller
- GUI2C display server: I2C target

This keeps one controller per I2C bus. Future multi-view displays can expose
multiple physical I2C ports, one per client/controller, and map each port to a
virtual view internally.

## Documents

- `docs/protocol.md`: packet format, command IDs, payload layouts and object semantics
- `docs/architecture.md`: repository split, I2C roles, sessions/views roadmap

## Current Protocol Scope

The current protocol draft covers:

- `PING`
- `HELLO_STRING`
- `DESTROY_ALL_OBJECTS`
- text objects
- rectangle objects
- line objects
- bar objects
- ring objects
- waveform objects

Planned protocol work includes explicit capability discovery, view reservation,
session IDs, present/update semantics, timeouts and release/reset behavior.
