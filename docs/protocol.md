<!--
SPDX-License-Identifier: MIT
Authors: any:frequency, seige.digital GbR, Leander Seige (2026)
-->

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

Payload layout:

- `u16 object_id`
- `u16 fg`
- `u16 bg`
- `u8 flags`
- `u8 zoom`
- `u8 text_len`
- `text_len` bytes text

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

Payload layout:

- `u16 object_id`
- `u16 fill`
- `u16 border`
- `u8 flags`

### `CREATE_LINE` (`0x16`)

Payload layout:

- `u16 object_id`
- `u16 x1`
- `u16 y1`
- `u16 x2`
- `u16 y2`
- `u16 fg`
- `u16 bg`
- `u8 thickness`

### `UPDATE_LINE` (`0x17`)

Payload layout:

- `u16 object_id`
- `u16 fg`
- `u16 bg`

### `CREATE_BAR` (`0x1A`)

Payload layout:

- `u16 object_id`
- `u16 x`
- `u16 y`
- `u16 w`
- `u16 h`
- `u16 fg`
- `u16 bg`
- `u16 value`
- `u8 direction`

### `UPDATE_BAR` (`0x1B`)

Payload layout:

- `u16 object_id`
- `u16 value`

Current server behavior:

- if geometry, colors and direction are unchanged, the server may update only the changed bar delta

### `CREATE_RING` (`0x1C`)

Payload layout:

- `u16 object_id`
- `u16 cx`
- `u16 cy`
- `u16 radius`
- `u16 inner_radius`
- `u16 opening_angle_deg`
- `u16 bg`
- `u16 fg_active`
- `u16 fg_inactive`
- `u16 value`

### `UPDATE_RING` (`0x1D`)

Payload layout:

- `u16 object_id`
- `u16 value`

Current server behavior:

- if geometry and colors are unchanged, the server may update only the ring sectors whose classification changed between the old and new value

### `CREATE_WAVEFORM` (`0x18`)

Payload layout:

- `u16 object_id`
- `u16 x`
- `u16 y`
- `u16 w`
- `u16 h`
- `u16 border`
- `u16 bg`
- `u8 flags`
- `u8 wave_count`
- `wave_count * u16 wave_color`

### `UPDATE_WAVEFORM` (`0x19`)

Common payload prefix:

- `u16 object_id`
- `u8 mode`

Mode `GUI2C_WAVEFORM_UPDATE_MODE_FULL_REPLACEMENT` (`0x01`):

- `u8 wave_index`
- `u8 sample_count`
- `sample_count * u16 sample`

Mode `GUI2C_WAVEFORM_UPDATE_MODE_SHIFTING` (`0x02`):

- `u8 wave_index`
- `u16 x_advance`
- `u16 sample`

Mode `GUI2C_WAVEFORM_UPDATE_MODE_SHIFTING_MULTI` (`0x03`):

- `u8 wave_count`
- `u16 x_advance`
- repeated `wave_count` times:
  - `u8 wave_index`
  - `u16 sample`

Current server behavior:

- `CREATE_TEXT`, `CREATE_RECT`, `CREATE_LINE`, `CREATE_BAR`, `CREATE_RING` and `CREATE_WAVEFORM` are idempotent
- resending a matching `CREATE_*` for an already existing object returns `OK` and leaves the object unchanged
- geometry and layout are currently defined by `CREATE_*` and remain fixed afterwards
- `UPDATE_TEXT` requires that the text object already exists
- `UPDATE_RECT` requires that the rect object already exists
- `UPDATE_LINE` requires that the line object already exists
- `UPDATE_BAR` requires that the bar object already exists, because the update payload contains only `object_id` and `value`
- `UPDATE_RING` requires that the ring object already exists, because the update payload contains only `object_id` and `value`
- `UPDATE_WAVEFORM` requires that the waveform object already exists, because the update payload does not contain geometry, border or color configuration

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

- `0x03`

## Rect flags

### `GUI2C_RECT_FLAG_FILL_TRANSPARENT`

Value:

- `0x01`

When set, the fill color is ignored and only the border may be drawn.

### `GUI2C_RECT_FLAG_BORDER_ENABLED`

Value:

- `0x02`

When set, a 1-pixel border is drawn inside the rectangle bounds.

## Bar directions

### `GUI2C_BAR_DIRECTION_TOP_TO_BOTTOM`

Value:

- `0x00`

### `GUI2C_BAR_DIRECTION_BOTTOM_TO_TOP`

Value:

- `0x01`

### `GUI2C_BAR_DIRECTION_LEFT_TO_RIGHT`

Value:

- `0x02`

### `GUI2C_BAR_DIRECTION_RIGHT_TO_LEFT`

Value:

- `0x03`

## Ring semantics

- `value` is interpreted as a `u16` fraction of the visible sweep
- filling starts at the lower apex of the circle and proceeds clockwise
- if `opening_angle_deg` is non-zero, that angle is left open symmetrically around the lower apex
- with an opening, filling begins at the left edge of the gap and proceeds clockwise over the remaining visible sweep
- `inner_radius = 0` produces a filled disk; larger values produce a ring

## Waveform flags

### `GUI2C_WAVEFORM_FLAG_BORDER_ENABLED`

Value:

- `0x01`

When set, a 1-pixel border is drawn around the widget.

## Waveform update modes

### `GUI2C_WAVEFORM_UPDATE_MODE_FULL_REPLACEMENT`

Value:

- `0x01`

The server replaces the stored samples for one wave, rescales over all currently stored waveform values in the widget and redraws. The exact redraw strategy is target-specific.

### `GUI2C_WAVEFORM_UPDATE_MODE_SHIFTING`

Value:

- `0x02`

The server advances stored samples for one wave by `x_advance`, drops values that moved out of view, appends the new sample at the right edge and redraws the widget. The exact redraw strategy is target-specific.

### `GUI2C_WAVEFORM_UPDATE_MODE_SHIFTING_MULTI`

Value:

- `0x03`

The server advances the stored history of multiple waves by the same `x_advance` and appends one new sample per referenced `wave_index` in a single packet. This reduces command overhead when several curves advance together.

## Rendering semantics

Commands are processed strictly in reception order.

In the current implementation, rendering is synchronous:

- one packet is received
- one command is fully executed
- visible changes are rendered and flushed
- only then is the next packet processed

This makes behavior simple and predictable, but also means that many small updates can be visually slower than one coarse update.

Line updates currently use this sequence:

- redraw the previous line using its stored `bg` color
- draw the new line using its `fg` color
- flush

Other object types may use more selective redraw strategies internally. For example, bars, rings and waveform widgets may update only the changed regions as long as the visible result still matches the public object semantics.

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
- `GUI2C_MAX_WAVEFORM_WAVES = 4`

These limits are compile-time constants from the public protocol header.

Waveform storage depth and the maximum number of waveform objects are intentionally not part of the public protocol contract. Those limits are server-internal and may vary by selected server target profile.

The same applies to server-internal rendering strategies such as column-wise waveform rendering, indexed caches or partial redraw acceleration paths.

## Notes for client authors

- keep object ids stable across updates
- prefer `UPDATE_*` over repeated destroy/create when the object is conceptually the same
- it is safe to resend `CREATE_*` during a resync phase because create is idempotent on the current server
- keep waveform create descriptors around on the client side because `UPDATE_WAVEFORM` cannot reconstruct a missing widget by itself
- keep text within `GUI2C_MAX_TEXT_LEN`
- for large animations, prefer changing only what really changed
