# How to Add a New Object Type

This guide describes the recommended order for adding a new GUI object type such as a line, icon or bitmap.

The public protocol should stay hardware-agnostic. Memory limits, reduced internal color formats and display-specific fast paths belong on the server side behind a target-profile or display-specific implementation detail.

## 1. Extend the protocol

Edit `include/gui2c/gui2c_protocol.h`:

- add new command ids such as `GUI2C_CMD_CREATE_FOO` and `GUI2C_CMD_UPDATE_FOO`
- add a public descriptor struct such as `gui2c_foo_desc_t`
- add any object-specific flags

This descriptor is the contract between client and server.

Keep that contract generic. Do not expose display-controller details such as internal framebuffer formats or per-target storage limits in the public descriptor unless they are truly part of the portable object model.

## 2. Extend the client API

Edit `include/gui2c/gui2c_client.h`:

- add `gui2c_client_create_foo(...)`
- add `gui2c_client_update_foo(...)`

Then implement both in `lib/client/gui2c_client.c`:

- serialize the descriptor into a compact payload
- send it with `gui2c_client_send_raw(...)`

Tip: keep the payload fixed-size if possible. It makes debugging much easier.

## 3. Add server-side storage

Edit `include/gui2c/gui2c_server.h`:

- define a `gui2c_foo_object_t`
- add storage for that object type to `gui2c_server_t`

For example, the current codebase keeps separate text and rect object arrays.

If the new object needs sizable caches or hardware-specific limits, introduce those through a server-side target profile rather than through `gui2c_protocol.h`.

## 4. Add allocation and lookup helpers

Edit:

- `lib/server/gui2c_server_objects.h`
- `lib/server/gui2c_server_objects.c`

Add functions such as:

- `gui2c_server_find_foo_object(...)`
- `gui2c_server_alloc_foo_object(...)`

Also update clear-all logic so `DESTROY_ALL_OBJECTS` removes the new object type.

## 5. Create the object module

Add:

- `lib/objects/gui2c_object_foo.h`
- `lib/objects/gui2c_object_foo.c`

This module should usually contain:

- payload parsing
- create
- update
- draw
- destroy

A good pattern is:

- parse payload into a public `gui2c_foo_desc_t`
- map that into an internal `gui2c_foo_object_t`
- draw using renderer/display primitives

Important: object logic belongs into `lib/objects/`, not `lib/renderer/`. The object decides what to draw. The renderer/display decides how to draw pixels or filled areas.

## 6. Hook it into the server dispatcher

Edit `lib/server/gui2c_server.c`:

- include the new object header
- handle the new commands in `gui2c_server_handle_packet(...)`
- include the new object type in redraw order

For layered scenes, define a stable draw order. A common rule is:

- background-like objects first
- text last

## 7. Add the source file to CMake

Edit `CMakeLists.txt` and add the new `.c` file to the `gui2c` library target.

## 8. Test with the demo client

Extend `firmware/client_object_demo_pico/main.c`:

- create the new object once
- update it in a loop
- verify create, update and destroy behavior

## 9. Optimize only after it works

First make it correct.

Then optimize:

- avoid unnecessary clears
- skip redraws when only color changes inside identical geometry
- prefer delta updates for value-only widgets so unchanged regions stay untouched
- batch SPI transfers for large fills
- avoid full-screen redraws unless necessary
- keep generic object semantics separate from display-specific fast paths
- put target-specific limits and internal buffer formats behind server profile headers or display-specific modules

## Checklist

Before calling the feature done, verify:

- commands compile and link
- payload parsing matches the client payload exactly
- create works
- update works
- destroy works
- `DESTROY_ALL_OBJECTS` removes the object type
- draw order is sensible
- the new `.c` file is included in CMake
