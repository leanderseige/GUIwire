# Porting Notes

GUI2C is intentionally split into layers:

- protocol/core
- client and server logic
- object modules
- renderer and display driver
- platform adapter
- firmware examples

The GUI2C core itself should remain free of MCU SDK includes.

## Layer responsibilities

### Protocol and core

Location:

- `include/gui2c/`
- `lib/core/`
- `lib/internal/`

Responsibilities:

- packet encoding and decoding
- protocol constants
- public descriptors
- shared low-level helpers

This layer should not know anything about a concrete MCU SDK.

### Client and server logic

Location:

- `lib/client/`
- `lib/server/`

Responsibilities:

- client-side payload creation
- server-side packet dispatch
- object storage
- redraw orchestration
- clear-all behavior

The public client API and protocol should remain portable across display types. Server-side target profiles may tune storage limits, caching strategies and internal pixel formats for a specific hardware class.

### Object modules

Location:

- `lib/objects/`

Responsibilities:

- per-object payload parsing
- create/update/draw/destroy logic
- mapping between public descriptors and internal object storage

Examples:

- `gui2c_object_text.*`
- `gui2c_object_rect.*`

Important:
a new GUI object type belongs here, not in the renderer.

### Renderer and display driver

Location:

- `lib/renderer/`
- `lib/display/`

Responsibilities:

- pixel and fill primitives
- glyph helpers
- controller-specific display initialization
- controller-specific transfer optimization
- optional controller-specific acceleration paths used by higher layers

The renderer/display layer does not define GUI object semantics.  
It only provides drawing primitives.

For targets with substantially different capabilities, prefer a selectable server target profile or display-specific optimization path over changing the public protocol.

### Platform adapter

Location:

- `platform/`

Responsibilities:

- MCU-specific SPI write
- MCU-specific GPIO output
- MCU-specific millisecond delay
- controller-side I2C write for the client

### Firmware examples

Location:

- `firmware/`

Responsibilities:

- board pin mapping
- actual bus setup
- interrupt setup
- main loops
- demo behavior

## What a new platform must provide

A new platform should implement:

- controller-side I2C write
- SPI write
- GPIO output
- millisecond delay

That is enough to support the current Pico-style client and display path.

## Recommended porting order

### 1. Bring up the platform adapter

Implement the transport helpers in `platform/<your-platform>/`.

At minimum you need:

- SPI write
- GPIO write
- delay
- I2C write on the client side

### 2. Verify one display driver

Start with an already supported display path such as ILI9488-style SPI display handling.

Before touching GUI objects, first verify:

- reset works
- backlight works
- full-screen fill works
- single-pixel drawing works
- text glyphs are visible

### 3. Bring up the server firmware

The server firmware must:

- initialize the display
- configure the GUI2C renderer context
- receive packets from the chosen transport
- pass complete packets to `gui2c_server_handle_packet(...)`

For transports like I2C slave reception, packet collection should happen promptly, ideally interrupt-driven.

### 4. Bring up a minimal client

A minimal client should:

- initialize its transport
- call `gui2c_client_init(...)`
- send `DESTROY_ALL_OBJECTS`
- create one text object

Do not start with animation.

First get a single static label visible.

### 5. Add update traffic

Once a static label works, add periodic `UPDATE_TEXT`.

Then test a rectangle object and its updates.

### 6. Optimize only after correctness

Typical optimization points are:

- SPI baud rate
- batching repeated fill data for large rectangles
- avoiding unnecessary clears on opaque object updates
- reducing tiny transfers
- using interrupt-driven or DMA-based transport handling
- tuning server-internal object limits per target profile
- choosing internal cache formats that match the target, such as indexed buffers on RAM-constrained displays or full offscreen buffers on faster hardware

## Common pitfalls

### Polling a slow receive path

If the server receives packets over I2C and only polls occasionally, large or back-to-back packets may be truncated or lost.

Use prompt polling at minimum.  
Interrupt-driven reception is better.

### Confusing object logic with renderer logic

A rectangle object is not a renderer feature.

Put:

- object semantics in `lib/objects/`
- drawing primitives in `lib/renderer/` or `lib/display/`

### Overwriting objects by draw order

Without a generic compositor, the last thing drawn wins.

Be explicit about redraw order, especially for background-like objects versus text.

### Excessive clears

For opaque objects with unchanged geometry, clearing before redrawing may be unnecessary and causes visible flicker.

On slower displays, consider server-side partial redraw strategies or internal caches before changing the client protocol. Those tradeoffs are usually target-specific.

### Missing header or CMake entries

When adding modules, check both:

- the source is compiled in `CMakeLists.txt`
- the right header is included where the symbols are used
