# GUI2C Protocol

## Packet format

All multi-byte integers are little-endian.

### Header

| Offset | Size | Meaning |
|---|---:|---|
| 0 | 1 | magic0 = 'G' |
| 1 | 1 | magic1 = '2' |
| 2 | 1 | protocol version |
| 3 | 1 | command |
| 4 | 2 | payload length |

The header is followed by `payload_len` payload bytes.

## Commands

### `PING` (`0x01`)
Payload: none

### `HELLO_STRING` (`0x02`)
Payload:
- bytes `0..n-1`: raw UTF-8 text bytes without terminating NUL

### `DESTROY_ALL_OBJECTS` (`0x10`)
Payload: none

Deletes all client-owned objects while preserving server object id `0`.

### `CREATE_TEXT` (`0x11`)
Payload:
- `u16 object_id`
- `u16 x`
- `u16 y`
- `u16 fg`
- `u16 bg`
- `u8 text_len`
- `text_len` bytes text

### `UPDATE_TEXT` (`0x12`)
Same payload layout as `CREATE_TEXT`.

If the object does not yet exist, the current server implementation creates it.

### `DESTROY_OBJECT` (`0x13`)
Payload:
- `u16 object_id`

## Rendering semantics

Commands are processed strictly in reception order.

In the current implementation, rendering is synchronous:
- one packet is received
- one command is fully executed
- visible changes are rendered and flushed
- only then is the next packet processed
