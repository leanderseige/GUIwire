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

## 5. Update objects by reusing the same object id

To change text content:

```c
text.text = "Counter 42";
gui2c_client_update_text(&client, &text);
```

To change rect fill color:

```c
rect.fill = gui2c_rgb565(255, 0, 0);
gui2c_client_update_rect(&client, &rect);
```

## 6. Keep IDs stable

The server identifies objects by `object_id`.

Use one fixed id per logical object. Do not create a new id every frame unless you really want a new object.

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
- slow the update rate temporarily
- look at the server serial log for packet status

## 10. Good first milestone

A very good first milestone for a new client is:

- one static title text
- one counter text updated once per second
- one rectangle whose fill color changes over time

If that works reliably, your transport and object pipeline are usually in good shape.
