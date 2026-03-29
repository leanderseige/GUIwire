# GUI2C Protocol

## Basics

GUI2C packets are binary and compact.

All multi-byte integers are little-endian.

A packet consists of:

- a fixed-size header
- a payload of `payload_len` bytes

## Header

| Offset | Size | Meaning |
|---|---:|---|
| 0 | 1 | magic0 = `'G'` |
| 1 | 1 | magic1 = `'2'` |
| 2 | 1 | protocol version |
| 3 | 1 | command |
| 4 | 2 | payload length |

The header is followed immediately by `payload_len` payload bytes.

## Commands

### `PING` (`0x01`)

Payload: none

Used for connectivity tests.

### `HELLO_STRING` (`0x02`)

Payload:

- bytes `0..n-1`: raw UTF-8 text bytes without terminating NUL

This is currently a simple diagnostic string.

### `DESTROY_ALL_OBJECTS` (`0x10`)

Payload: none

Deletes all client-owned objects while preserving server object id `0`.

### `CREATE_TEXT` (`0x11`)

Payload layout:

- `u16 object_id`
- `u16 x`
- `u16 y`
- `u16 w`
- `u16 h`
- `u16 fg`
- `u16 bg`
- `u8 flags`
- `u8 zoom`
- `u8 text_len`
- `text_len` bytes text

### `UPDATE_TEXT` (`0x12`)

Same payload layout as `CREATE_TEXT`.

Current server behavior:
if the object does not yet exist, the current implementation creates it.

### `DESTROY_OBJECT` (`0x13`)

Payload:

- `u16 object_id`

The server destroys the matching object by id.

### `CREATE_RECT` (`0x14`)

Payload layout:

- `u16 object_id`
- `u16 x`
- `u16 y`
- `u16 w`
- `u16 h`
- `u16 fill`
- `u16 border`
- `u8 flags`

### `UPDATE_RECT` (`0x15`)

Same payload layout as `CREATE_RECT`.

Current server behavior:
if the object does not yet exist, the current implementation creates it.

## Colors

Colors use `gui2c_color_t`, currently RGB565 in public descriptors and payloads.

Examples:

- `gui2c_rgb565(255, 0, 0)` = red
- `gui2c_rgb565(0, 255, 0)` = green
- `gui2c_rgb565(0, 0, 255)` = blue

## Text flags

### `GUI2C_TEXT_FLAG_BG_TRANSPARENT`

When set, the text background is not filled before glyph rendering.

When clear, the text object's `bg` color is used to fill its bounding box.

Recommended value:

- `0x04`

## Rect flags

### `GUI2C_RECT_FLAG_FILL_TRANSPARENT`

Value:

- `0x01`

When set, the fill color is ignored and only the border may be drawn.

### `GUI2C_RECT_FLAG_BORDER_ENABLED`

Value:

- `0x02`

When set, a 1-pixel border is drawn inside the rectangle bounds.

## Rendering semantics

Commands are processed strictly in reception order.

In the current implementation, rendering is synchronous:

- one packet is received
- one command is fully executed
- visible changes are rendered and flushed
- only then is the next packet processed

This makes behavior simple and predictable, but also means that many small updates can be visually slower than one coarse update.

## Object ownership

Rules:

- object id `0` is reserved for the server
- client objects should use ids `>= 1`
- `DESTROY_ALL_OBJECTS` removes only client-owned objects

## Packet size limits

Current public limits include:

- `GUI2C_MAX_PACKET_SIZE = 256`
- `GUI2C_MAX_TEXT_LEN = 64`
- `GUI2C_MAX_OBJECTS = 32`

These limits are compile-time constants from the public protocol header.

## Notes for client authors

- keep object ids stable across updates
- prefer `UPDATE_*` over repeated destroy/create when the object is conceptually the same
- keep text within `GUI2C_MAX_TEXT_LEN`
- for large animations, prefer changing only what really changed
