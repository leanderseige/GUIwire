<!--
SPDX-License-Identifier: MIT
Authors: any:frequency, seige.digital GbR, Leander Seige (2026)
-->

# GUIwire Architecture

## Specification boundary

[`GUIwire.md`](../GUIwire.md) is the authoritative document. This file
explains how an implementation should be split; it does not introduce a
second wire format.

The repository contains the protocol definition and a portable C reference
library. Its public packet structures are derived from the specification rather
than defining private extensions as if they were standard GUIwire.

The library is split into two reusable runtimes:

- `guiwire_host`: discovery, polling, timeout handling, hot-patch recovery,
  object ownership, generic rendering and link-rate negotiation
- `guiwire_device_runtime`: PING/POLL handling, response scheduling, queue
  delivery, link-rate negotiation and Device session expiry

The lower-level `guiwire_core` and `guiwire_device` targets can also be used
directly when an application needs a custom scheduler.

A port of the reliable Host runtime supplies a fresh eight-byte token for
each discovery attempt through `new_session_token`; the Pico port uses the
Pico SDK random source. Setting `require_reliable_session` rejects
untagged legacy peers instead of silently losing client-switch detection.
The optional `read_time_ms` callback lets a slow display backend refresh
link deadlines after synchronous drawing. Device ports need UART byte I/O,
turnaround hooks and (for adaptive mode) a baud-change hook; they do not need
a random-number source.

## Roles

GUIwire has two application roles:

- **Host**: provides the display and optional controls. It schedules traffic,
  renders GUI objects and owns the connected user-interface session.
- **Device**: is the functional module. It owns the functional state and sends
  the interface objects and state changes that the Host should present.

The Host is the link scheduler. The Device only transmits after a Host frame
has granted it a response opportunity. This makes a single shared conductor
deterministic and avoids collisions without a network arbitration scheme.

## Single connection

The initial profile supports exactly one Host and one Device:

```text
┌──────────────┐       TS patch cable       ┌──────────────┐
│ Device       │ DATA + GND, half duplex    │ Host         │
│ functional   │◄─────────────────────────►│ display/UI   │
└──────────────┘                            └──────────────┘
```

The cable carries data only. Both modules have their own power supply and
local MCU logic voltage. A display controller may use any local display bus;
that bus is not part of GUIwire.

## Session sequence

After connection or a timeout, the Host discards its partial receive frame and
starts discovery with `PING`:

```text
Host                                      Device

PING(session token) ────────────────────────►
             ◄────────────────────────────── PING(echo token)
POLL(token, ACK) ───────────────────────────►
             ◄──────── SESSION_DATA(seq, GUI command)
POLL(token, seq) ───────────────────────────► (ACK commits queue head)
             ◄──────── next SESSION_DATA or SESSION_EMPTY
GET_LINK_INFO(token) ───────────────────────►
             ◄──────── LINK_INFO(token, common rates)
SET_BAUD(token, next higher rate) ───────────►
             ◄──────── BAUD_ACK at old rate, then both switch
POLL(token, ACK) ───────────────────────────►
             ◄──────── session-tagged response validates new rate
```

The Device queues its current interface and subsequent state changes. One
`POLL` obtains at most one session-tagged application packet. The Device
retains that packet until the next `POLL` acknowledges its sequence. A Host
repeats `POLL` until it receives `SESSION_EMPTY`; it continues to poll while
the session remains active.

The rate exchange is optional for compatibility. Both endpoints begin at the
9600-baud startup rate; a legacy Device that does not answer `GET_LINK_INFO`
is simply used at that rate. The adaptive Host first builds the GUI and then
tests common higher rates one step at a time in normal request/response slots.
Three valid `POLL` responses confirm a step. A failed switch is recovered by
the Device's fallback timer and the Host's base-rate retry without clearing
the scene unless verification at the base rate also fails.

This schedule preserves the object command profile while keeping line
ownership explicit. A Device must not start a transmission spontaneously.

## Hot-patch handover

The cable has no presence signal. At the base rate, the Host detects removal
through missed responses and clears the old Device scene before discovery.
At a higher rate it first tries silent 9600-baud fallback so a rate-only
failure need not erase the GUI; failed base-rate verification then clears the
scene. The previous Device expires its session after its configured timeout
without authenticated Host frames (2500 ms in the adaptive reference Device)
and drops its pending
transmit queue.

If another already-running Device is attached at the same rate, it rejects
the old session token with `SESSION_MISMATCH`. The Host immediately clears the
old scene and starts a new `PING`. At a different rate, the Host first
performs base-rate recovery, then detects the token mismatch. The reference
Host requires token-aware peers; an untagged legacy reply is not treated as
a robust GUIwire hot-patch session.

The newly connected Device answers `PING` and begins its first response queue
with `DESTROY_ALL_OBJECTS`, then sends its identity, descriptors and current
state. The Host does not render object packets before that reset command and
polls until `SESSION_EMPTY`. Thus the Host, the previous Device and the new Device
all reach a defined state when one cable is moved.

The Host caches object descriptors and current values independently of the
physical renderer. The local debug button temporarily routes rendering to a
no-op backend while protocol updates continue to modify that cache. On
release, the Host redraws from the cache; the Device need not resend the
scene. A real disconnect or client switch invalidates the cache.

## Layering

Implementations should keep these layers separate:

1. **Physical layer**: 3.5 mm TS connection, protected single-wire
   open-collector-style signaling and local TX/RX conversion.
2. **Link layer**: asynchronous byte transport, escaping, frame length,
   CRC-16, turnaround and recovery.
3. **Application layer**: packet header, commands, object ownership and
   rendering semantics.

The Host's display technology, font renderer and local graphics API are
implementation details. They must not change the bytes sent by a Device.

## Porting boundary

The portable library deliberately stops at callbacks. A manufacturer supplies
the following implementation-specific pieces:

```text
GUIwire library
  ├── protocol, CRC, framing and escaping
  ├── Host/Device sessions and hot-patching
  ├── object serialization and generic object rendering
  └── compile-time bounded queues and scene storage

manufacturer port
  ├── write one byte / release the half-duplex line
  ├── receive bytes and provide a millisecond clock to *_feed_byte/*_tick
  ├── render fill_rect / draw_pixel / flush
  └── implement functional module state
```

The reference RP2040 transport is isolated in `firmware/ports/pico`; it is
not included by the portable `firmware/common` CMake target. This keeps the
same library usable from AVR-GCC, Pico SDK, bare-metal projects or an RTOS.

## Future work

Reserved areas include richer capability discovery, semantic widgets,
interaction events and batching/present operations.
Such additions must preserve the one-Host/one-Device schedule and must be
assigned new versioned commands or capability bits.
