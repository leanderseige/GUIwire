# How to Write Your Own Client

A GUI2C client only needs a transport write function and some object descriptors.

## 1. Provide a transport

Create a `gui2c_transport_t` with:

- `write`
- `ctx`

Example:

```c
gui2c_transport_t transport;
transport.write = my_transport_write;
transport.ctx = &my_bus;
```

The write callback receives a fully encoded GUI2C packet.

## 2. Initialize the client

```c
gui2c_client_t client;
gui2c_client_init(&client, transport);
```

## 3. Optionally clear the server state

A common startup sequence is:

```c
gui2c_client_destroy_all_objects(&client);
```

That removes all client-owned objects on the server.

If your client may start before the server, it is also reasonable to keep the current `CREATE_*` descriptors in RAM so they can be resent during a resync phase. The current server treats repeated `CREATE_*` commands as idempotent.

## 4. Create objects

For text:

```c
gui2c_text_desc_t text;
text.object_id = 10;
text.x = 8;
text.y = 24;
text.w = 240;
text.h = 20;
text.fg = gui2c_rgb565(255, 255, 255);
text.bg = gui2c_rgb565(0, 0, 64);
text.flags = 0;
text.zoom = 2;
text.text = "Hello GUI2C";
gui2c_client_create_text(&client, &text);
```

For a rectangle:

```c
gui2c_rect_desc_t rect;
rect.object_id = 100;
rect.x = 100;
rect.y = 100;
rect.w = 120;
rect.h = 80;
rect.fill = gui2c_rgb565(0, 255, 255);
rect.border = gui2c_rgb565(255, 255, 255);
rect.flags = GUI2C_RECT_FLAG_BORDER_ENABLED;
gui2c_client_create_rect(&client, &rect);
```

For a line:

```c
gui2c_line_desc_t line;
line.object_id = 200;
line.x1 = 20;
line.y1 = 40;
line.x2 = 180;
line.y2 = 120;
line.fg = gui2c_rgb565(255, 0, 0);
line.bg = gui2c_rgb565(0, 0, 0);
line.thickness = 3;
gui2c_client_create_line(&client, &line);
```

For a bar:

```c
gui2c_bar_desc_t bar;
bar.object_id = 250;
bar.x = 200;
bar.y = 40;
bar.w = 80;
bar.h = 12;
bar.fg = gui2c_rgb565(0, 200, 255);
bar.bg = gui2c_rgb565(16, 16, 16);
bar.value = 32768;
bar.direction = GUI2C_BAR_DIRECTION_LEFT_TO_RIGHT;
gui2c_client_create_bar(&client, &bar);
```

For a ring:

```c
gui2c_ring_desc_t ring;
ring.object_id = 260;
ring.cx = 250;
ring.cy = 90;
ring.radius = 40;
ring.inner_radius = 24;
ring.opening_angle_deg = 60;
ring.bg = gui2c_rgb565(0, 0, 0);
ring.fg_active = gui2c_rgb565(255, 220, 0);
ring.fg_inactive = gui2c_rgb565(40, 40, 40);
ring.value = 32768;
gui2c_client_create_ring(&client, &ring);
```

For a waveform:

```c
gui2c_color_t wave_colors[2] = {
    gui2c_rgb565(0, 255, 0),
    gui2c_rgb565(255, 255, 0)
};
gui2c_waveform_desc_t wf;
wf.object_id = 300;
wf.x = 10;
wf.y = 140;
wf.w = 220;
wf.h = 80;
wf.border = gui2c_rgb565(255, 255, 255);
wf.bg = gui2c_rgb565(0, 0, 0);
wf.flags = GUI2C_WAVEFORM_FLAG_BORDER_ENABLED;
wf.wave_count = 2;
wf.wave_colors = wave_colors;
gui2c_client_create_waveform(&client, &wf);
```

## 5. Update objects by reusing the same object id

To change text content:

```c
gui2c_text_update_t text_update;
text_update.object_id = text.object_id;
text_update.fg = text.fg;
text_update.bg = text.bg;
text_update.flags = text.flags;
text_update.zoom = text.zoom;
text_update.text = "Counter 42";
gui2c_client_update_text(&client, &text_update);
```

To change rect fill color:

```c
gui2c_rect_update_t rect_update;
rect_update.object_id = rect.object_id;
rect_update.fill = gui2c_rgb565(255, 0, 0);
rect_update.border = rect.border;
rect_update.flags = rect.flags;
gui2c_client_update_rect(&client, &rect_update);
```

To change a line color:

```c
gui2c_line_update_t line_update;
line_update.object_id = line.object_id;
line_update.fg = gui2c_rgb565(255, 255, 0);
line_update.bg = line.bg;
gui2c_client_update_line(&client, &line_update);
```

The current object model treats geometry as create-time state. If you need a different position or size, destroy and recreate the object for now.

To change a bar value:

```c
gui2c_bar_update_t bar_update;
bar_update.object_id = 250;
bar_update.value = 50000;
gui2c_client_update_bar(&client, &bar_update);
```

On current servers this is typically cheap, because only the changed fill delta needs to be redrawn.

To change a ring value:

```c
gui2c_ring_update_t ring_update;
ring_update.object_id = 260;
ring_update.value = 50000;
gui2c_client_update_ring(&client, &ring_update);
```

On current servers this is also a value-only delta update. Unchanged ring sectors do not need to be redrawn.

To replace one waveform completely:

```c
uint16_t samples[8] = {10, 20, 40, 80, 60, 30, 15, 5};
gui2c_waveform_full_update_t wf_full;
wf_full.object_id = 300;
wf_full.wave_index = 0;
wf_full.sample_count = 8;
wf_full.samples = samples;
gui2c_client_update_waveform_full(&client, &wf_full);
```

To append one shifting sample:

```c
gui2c_waveform_shift_update_t wf_shift;
wf_shift.object_id = 300;
wf_shift.wave_index = 1;
wf_shift.x_advance = 2;
wf_shift.sample = 1234;
gui2c_client_update_waveform_shift(&client, &wf_shift);
```

To append several shifting samples with the same `x_advance` in one packet:

```c
gui2c_waveform_shift_multi_update_t wf_multi;
uint8_t wave_indices[2];
uint16_t samples[2];

wave_indices[0] = 0;
wave_indices[1] = 1;
samples[0] = 1500;
samples[1] = 2300;

wf_multi.object_id = 300;
wf_multi.wave_count = 2;
wf_multi.x_advance = 1;
wf_multi.wave_indices = wave_indices;
wf_multi.samples = samples;

gui2c_client_update_waveform_shift_multi(&client, &wf_multi);
```

This multi-wave form is usually the best choice when several curves advance together, because it reduces packet overhead and gives the server more freedom to use a single optimized redraw path.

## 6. Keep IDs stable

The server identifies objects by `object_id`.

Use one fixed id per logical object. Do not create a new id every frame unless you really want a new object.

For resync handling, keep the same ids and resend the corresponding `CREATE_*` commands instead of inventing replacement ids.

## 7. Check return values

Always check `gui2c_status_t` from client calls during development. It helps catch payload-length issues and transport failures early.

## 8. Start simple

A minimal custom client usually does only this:

- init bus
- init `gui2c_client_t`
- destroy all old objects
- create one text object
- update it in a loop

That is the easiest way to verify that your transport and timing are correct.

## 9. Debugging tips

If a client command seems to do nothing:

- check the return value of the client function
- verify object ids are not accidentally changing
- make sure text length stays within `GUI2C_MAX_TEXT_LEN`
- make sure waveform `wave_count` stays within `GUI2C_MAX_WAVEFORM_WAVES`
- slow the update rate temporarily
- look at the server serial log for packet status

Remember that color precision and some rendering details are server-side implementation choices. The client sends desired geometry and colors; the server may map those to the best match for the selected display hardware.

The same rule applies to performance optimizations: partial redraws, column-wise waveform rendering, indexed internal buffers or display-specific acceleration are server-side choices and do not require protocol changes.

## 10. Good first milestone

A very good first milestone for a new client is:

- one static title text
- one counter text updated once per second
- one rectangle whose fill color changes over time
- one line that moves across the screen

If that works reliably, your transport and object pipeline are usually in good shape.
