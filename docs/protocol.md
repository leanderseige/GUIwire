<!--
SPDX-License-Identifier: MIT
Authors: any:frequency, seige.digital GbR, Leander Seige (2026)
-->

# GUIwire Protocol

This document is the detailed packet registry for the normative rules in
[`GUIwire.md`](../GUIwire.md). All multi-byte integers are unsigned unless
stated otherwise and are encoded little-endian.

## Physical and link profile

The standard transport is an asynchronous byte stream over one shared DATA
conductor and GND. The mandatory startup rate is **9600 baud, 8 data bits, no
parity, one stop bit (8N1)**. Optional rates are negotiated at the application
layer after `PING`; the demonstrator tests 19200, 38400 and 57600 baud. The
transceiver may invert the signal internally; the application bytes are not
inverted.

The Host schedules every exchange. A transmitter releases DATA after its stop
bit. The other participant waits at least two character times before
transmitting. A receiver must abandon an incomplete frame after a session
timeout and resume searching for `SYNC`.

### Link frame

After `SYNC`, the following bytes are escaped as one body:

```text
┌──────┬────────────┬───────────────┬────────┐
│ SYNC │ frame_len │ application   │ CRC-16 │
│ 0x7E │ u16 LE    │ packet        │ u16 LE │
└──────┴────────────┴───────────────┴────────┘
```

`frame_len` is the number of unescaped bytes in the application packet. It
includes the six-byte application header and its payload. Valid values are
`6..256`. The CRC is CRC-16/CCITT-FALSE, initialized to `0xFFFF`, calculated
over the unescaped `frame_len` field followed by the complete application
packet, and appended least-significant byte first.

Byte stuffing is applied to `frame_len`, the application packet and the CRC:

- `0x7E` becomes `0x7D 0x5E`
- `0x7D` becomes `0x7D 0x5D`

Any other `0x7D xx` sequence is invalid. A new `SYNC` abandons the current
candidate frame and starts a new one. CRC failures and invalid lengths are
discarded without executing the application command.

### Application packet

```text
┌────────┬────────┬─────────┬────────┬────────────┬─────────┐
│ magic0 │ magic1 │ version │ command│ payload_len│ payload │
│  'G'   │  '2'   │   u8    │  u8    │    u16 LE  │   ...   │
└────────┴────────┴─────────┴────────┴────────────┴─────────┘
```

`magic0` and `magic1` are ASCII `G` and `2`. The second byte is retained
for application-packet compatibility; it does not describe the GUIwire
electrical layer. `version` is `0x01` for this document. `payload_len`
must equal the remaining packet length. Unknown versions and commands are
ignored after the frame has been validated.

## Turnaround and scheduling

The Host sends a request and then releases the line. The Device may send one
response frame after the turnaround interval. In a reliable session, `PING`
receives a token-echoing `PING` response; `POLL` receives one `SESSION_DATA`
or `SESSION_EMPTY` frame. The older empty-payload request/reply form remains
parsable for compatibility but cannot guarantee client-switch detection.

The Host must not start another frame until the Device has released the line
or the response timeout has elapsed. The timeout must cover the longest
accepted frame, including worst-case byte stuffing and turnaround. With a
256-byte packet limit at 9600 baud, the serialized response alone can take
about 543 ms; the reference Host allows at least another 75 ms of margin.

On disconnect, reset or timeout, both sides discard partial frames. The Device
rebuilds its queued interface from its current state after a new `PING`.
The Host treats a fresh, matching `PING` response as a new session and clears
all cached objects. A baud-rate recovery in the *same* session does not clear
the cache.

## Hot-patch session lifecycle

The TS cable has no presence conductor. An unplugged DATA line is
indistinguishable from an idle line, so presence is established by valid
request/response traffic.

While connected, the Host should send `POLL` at least every 100 ms. At the
startup rate, it declares the session lost after three consecutive response
timeouts or its configured session timeout without a valid response. The
reference Host uses 1500 ms at the base rate. It then stops interaction
traffic, discards partial input, clears all Device-owned objects and restarts
discovery with `PING`. The display should show a neutral
no-device state while discovery is pending. At a negotiated higher rate, it
first attempts the silent base-rate recovery described below, because a rate
failure alone does not prove that the Device was unplugged.

The Device expires its session after its configured timeout without an
authenticated Host frame. It stops transmitting, discards its pending GUI
queue and marks the
session inactive. The Device's functional state is not reset by this timeout.
When rate fallback is enabled, the session timeout must exceed the negotiated
silent fallback interval plus the Host's recovery margin; the reference
adaptive Device uses 2500 ms. The fixed-rate demonstrator uses 1500 ms so a
local debug redraw does not normally invalidate the session.

Every reliable session uses a fresh eight-byte Host-generated token. The Host
places it in `PING`; the Device echoes it. Subsequent `POLL`,
`GET_LINK_INFO`, and `SET_BAUD` requests carry that token. The Device does not
execute requests for another session. Instead it returns `SESSION_MISMATCH`
with the token from the rejected request. On a matching mismatch response,
the Host discards the old scene and starts a new `PING`. A client swap at the
same baud is therefore recognized even if the new client is already running
and responding. If the baud differs, the Host first performs base-rate
recovery and then verifies the session token. A Host requiring this guarantee
must reject legacy empty-payload `PING` replies.

After a new `PING` response, the new Device must queue the following initial
sequence:

1. `DESTROY_ALL_OBJECTS`
2. `HELLO_STRING`
3. all current `CREATE_*` descriptors
4. current `UPDATE_*` state packets, where required

The Host must not render Device-owned object packets until it has received the
initial `DESTROY_ALL_OBJECTS`. It renders each subsequent object command as
it is received; `SESSION_EMPTY` means the initial scene is complete. This
ordering makes a cable move from Device A to Device B
safe even when Device A was interrupted in the middle of a frame.

The cable itself has no state machine; the Host, the previous Device and the
new Device perform the handover through token mismatch or timeout, discovery
and resynchronization. While a local debug view is shown, the Host continues
to apply confirmed GUI updates to its cached object model without drawing
them. On release it redraws that model locally, without requesting a new
Device snapshot. A true disconnect or client switch invalidates the cache.

## Command registry

| Command | Value | Direction | Payload |
|---|---:|---|---|
| `PING` | `0x01` | Host ↔ Device | 8-byte session token (legacy: none) |
| `HELLO_STRING` | `0x02` | Device → Host | raw UTF-8 bytes |
| `POLL` | `0x03` | Host → Device | session token, `u16 ACK` (legacy: none) |
| `NO_DATA` | `0x04` | Device → Host | none; legacy only |
| `GET_LINK_INFO` | `0x05` | Host → Device | session token (legacy: none) |
| `LINK_INFO` | `0x06` | Device → Host | session token, `u8 count`, then `count × u32 baud` |
| `SET_BAUD` | `0x07` | Host → Device | session token, `u32 baud`, `u8 rate_token` |
| `BAUD_ACK` | `0x08` | Device → Host | session token, `u32 baud`, `u8 rate_token` |
| `SESSION_DATA` | `0x09` | Device → Host | session token, `u16 seq`, `u8 inner_command`, inner payload |
| `SESSION_EMPTY` | `0x0A` | Device → Host | session token |
| `SESSION_MISMATCH` | `0x0B` | Device → Host | rejected request's session token |
| `DESTROY_ALL_OBJECTS` | `0x10` | Device → Host | none |
| `CREATE_TEXT` | `0x11` | Device → Host | text descriptor |
| `UPDATE_TEXT` | `0x12` | Device → Host | text state |
| `DESTROY_OBJECT` | `0x13` | Device → Host | `u16 object_id` |
| `CREATE_RECT` | `0x14` | Device → Host | rectangle descriptor |
| `UPDATE_RECT` | `0x15` | Device → Host | rectangle state |
| `CREATE_LINE` | `0x16` | Device → Host | line descriptor |
| `UPDATE_LINE` | `0x17` | Device → Host | line state |
| `CREATE_WAVEFORM` | `0x18` | Device → Host | waveform descriptor |
| `UPDATE_WAVEFORM` | `0x19` | Device → Host | waveform update |
| `CREATE_BAR` | `0x1A` | Device → Host | bar descriptor |
| `UPDATE_BAR` | `0x1B` | Device → Host | bar state |
| `CREATE_RING` | `0x1C` | Device → Host | ring descriptor |
| `UPDATE_RING` | `0x1D` | Device → Host | ring state |

The Device queues packets in order. A Host executes only complete, CRC-valid
packets bound to the active session. The object commands are deliberately
retained as an immediate-mode
rendering profile; a future semantic widget profile will use new commands.

### `PING` (`0x01`)

In reliable mode the Host generates an unpredictable eight-byte token for
every new attempt and the Device echoes the exact eight bytes. This establishes
the session identity. The legacy form has an empty payload in both directions.

### `HELLO_STRING` (`0x02`)

Payload: raw UTF-8 bytes without a terminating NUL. This is a diagnostic
identity string, not a required manufacturer/product registry.

### Reliable `POLL` and responses (`0x03`, `0x09`..`0x0B`)

In reliable mode `POLL` carries `token[8], u16 ACK`. The first ACK is
`0xFFFF` (none yet). A `SESSION_DATA` response carries
`token[8], u16 seq, u8 inner_command, inner_payload`. The first sequence is
zero. The Host checks the token and executes a new sequence at most once,
then puts that sequence into its next `POLL` as the ACK. Until the Device sees
that ACK it keeps the queue head and retransmits it on every valid `POLL`.
Upon ACK, it removes the head and advances the sequence. Sequence `0xFFFF`
is skipped, so it remains an unambiguous no-ACK sentinel. A duplicate
sequence does not execute the enclosed command again. An unexpected sequence
or invalid inner command forces a fresh session, not a silent queue advance.
If no object is queued the Device responds `SESSION_EMPTY` with `token[8]`.
The first object of a new session must be `DESTROY_ALL_OBJECTS`; an empty
initial snapshot is a protocol error and triggers rediscovery.

The envelope adds 11 bytes to an object packet. A Device must limit an
enclosed object packet to `MAX_PACKET_SIZE - 11`; the ATmega demonstrator uses
`MAX_PACKET_SIZE=98` and a nine-entry queue. This exactly fits a 64-byte
`CREATE_TEXT` in the session envelope. The Host may use a larger frame
limit. The older `POLL`/`NO_DATA` exchange has empty payloads and no ACK.

### Link-rate negotiation (`0x05`..`0x08`)

`GET_LINK_INFO` carries the current session token. The Device answers with
`LINK_INFO`:

```text
token[8], u8 count, u32 baud[count]
```

The baud values are little-endian and `count` must be between 1 and 8.
`SET_BAUD` is sent at the current rate:

```text
token[8], u32 baud, u8 rate_token
```

The Device sends `BAUD_ACK` with the same values at the current rate and only
then changes its UART configuration. The Host changes its UART after receiving
and validating that acknowledgement. The one-byte rate token is nonzero and
lets each side discard stale rate acknowledgements. The eight-byte session
token independently prevents another client from accepting a stale Host
request. A failed or unsupported selection is not acknowledged. Both sides must retain a fallback
to the 9600-baud startup rate.

The reference Host exchanges ordinary GUI `POLL`/response frames during rate
discovery and between tests. After its initial scene is present it attempts
the next higher *common* rate, rather than jumping immediately to the fastest
advertised rate. A step is confirmed by three consecutive valid `POLL`
responses at that rate. Another step is not attempted for 5 seconds. A valid
link that degrades may step down one common rate via the same handshake.
There are no concurrent UART rates: each test occupies an exclusive
half-duplex turn.

The adaptive Device returns to 9600 baud after 400 ms without a valid Host
frame at a higher rate, retaining its GUI queue. After a lost `BAUD_ACK` or
two unanswered high-rate polls, the reference Host stays silent for 450 ms,
then uses 9600 baud and verifies it with `POLL`. A failed or downshifted rate
is not tried again for 30 seconds. The Host retains the rendered GUI through
these rate trials; if base-rate verification also fails, normal hot-patch
session loss and discovery apply. Implementations using different timeout
values must ensure the Device stays in session until the Host can verify the
base rate.

### `DESTROY_ALL_OBJECTS` (`0x10`)

Payload: none. The Host deletes all Device-owned objects. Object id `0` is
reserved for Host-owned content.

### Text

`CREATE_TEXT` (`0x11`) payload:

```text
u16 object_id, u16 x, u16 y, u16 w, u16 h,
u16 fg, u16 bg, u8 flags, u8 zoom, u8 text_len, text[text_len]
```

`UPDATE_TEXT` (`0x12`) payload:

```text
u16 object_id, u16 fg, u16 bg, u8 flags, u8 zoom,
u8 text_len, text[text_len]
```

Text is UTF-8. `text_len` is a byte count and must not exceed 64.

### Rectangle

`CREATE_RECT` (`0x14`) payload:

```text
u16 object_id, u16 x, u16 y, u16 w, u16 h,
u16 fill, u16 border, u8 flags
```

`UPDATE_RECT` (`0x15`) payload:

```text
u16 object_id, u16 fill, u16 border, u8 flags
```

### Line

`CREATE_LINE` (`0x16`) payload:

```text
u16 object_id, u16 x1, u16 y1, u16 x2, u16 y2,
u16 fg, u16 bg, u8 thickness
```

`UPDATE_LINE` (`0x17`) payload:

```text
u16 object_id, u16 fg, u16 bg
```

### Bar

`CREATE_BAR` (`0x1A`) payload:

```text
u16 object_id, u16 x, u16 y, u16 w, u16 h,
u16 fg, u16 bg, u16 value, u8 direction
```

`UPDATE_BAR` (`0x1B`) payload:

```text
u16 object_id, u16 value
```

### Ring

`CREATE_RING` (`0x1C`) payload:

```text
u16 object_id, u16 cx, u16 cy, u16 radius, u16 inner_radius,
u16 opening_angle_deg, u16 bg, u16 fg_active, u16 fg_inactive,
u16 value
```

`UPDATE_RING` (`0x1D`) payload:

```text
u16 object_id, u16 value
```

`value` is a `u16` fraction of the visible sweep. Filling starts at the
lower apex and proceeds clockwise. A non-zero opening is symmetric around the
lower apex. `inner_radius = 0` produces a filled disk.

### Waveform

`CREATE_WAVEFORM` (`0x18`) payload:

```text
u16 object_id, u16 x, u16 y, u16 w, u16 h,
u16 border, u16 bg, u8 flags, u8 wave_count,
wave_count × u16 wave_color
```

`UPDATE_WAVEFORM` (`0x19`) begins with `u16 object_id, u8 mode`.

For `FULL_REPLACEMENT` (`0x01`): `u8 wave_index, u8 sample_count`, followed
by `sample_count × u16 sample`.

For `SHIFTING` (`0x02`): `u8 wave_index, u16 x_advance, u16 sample`.

For `SHIFTING_MULTI` (`0x03`): `u8 wave_count, u16 x_advance`, followed by
`wave_count` repetitions of `u8 wave_index, u16 sample`.

## Flags and enumerations

- `TEXT_FLAG_BG_TRANSPARENT = 0x03`: do not fill the text bounds.
- `RECT_FLAG_FILL_TRANSPARENT = 0x01`: ignore the fill colour.
- `RECT_FLAG_BORDER_ENABLED = 0x02`: draw a one-pixel inner border.
- `WAVEFORM_FLAG_BORDER_ENABLED = 0x01`: draw a one-pixel border.

Bar directions are `TOP_TO_BOTTOM = 0x00`, `BOTTOM_TO_TOP = 0x01`,
`LEFT_TO_RIGHT = 0x02` and `RIGHT_TO_LEFT = 0x03`.

Colours are RGB565. A public implementation may expose helpers such as
`guiwire_rgb565(255, 0, 0)`; the wire representation remains a `u16`.

## Object and rendering semantics

- Object id `0` is reserved for Host-owned content; Device objects use ids
  `1..32`.
- `CREATE_*` is idempotent for an existing matching object.
- Geometry and layout are established by `CREATE_*` and remain fixed until the
  object is destroyed.
- `UPDATE_*` requires the object to exist.
- Commands are rendered in reception order. A Host may redraw selectively, but
  the visible result must match the object semantics.
- For resynchronization, a Device should enqueue `DESTROY_ALL_OBJECTS` followed
  by its current descriptors and state updates.

## Limits

The current profile defines:

```text
GUIWIRE_MAX_PACKET_SIZE   = 256   (application bytes, header included)
GUIWIRE_MAX_TEXT_LEN       = 64
GUIWIRE_MAX_OBJECTS        = 32
GUIWIRE_MAX_WAVEFORM_WAVES = 4
```

Waveform history depth and renderer caches are Host-internal limits. They are
not part of wire compatibility.
