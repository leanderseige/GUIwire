# GUI2C Fresh

A clean restart of the GUI2C project with an object-oriented protocol.

## Current scope

This repository currently provides:

- a compact binary GUI2C protocol
- client-managed object IDs
- server-reserved object ID 0 for the boot/status label
- text objects with create/update/destroy support
- `DESTROY_ALL_OBJECTS` as a clear-screen-style command
- Pico platform adapters for I2C/SPI/GPIO/delay
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

Rules:

- object id `0` is reserved for the server
- client objects should use ids `>= 1`
- `DESTROY_ALL_OBJECTS` removes only client-owned objects

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
