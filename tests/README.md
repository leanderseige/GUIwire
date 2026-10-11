# GUIwire protocol loopback

From the repository root, compile and run the portable half-duplex
request/response test with the ATmega demonstrator's packet and queue sizes:

```sh
cc -std=c11 -O2 -DGUIWIRE_DEVICE_QUEUE_CAPACITY=10u \
  -DGUIWIRE_MAX_PACKET_SIZE=96u \
  -Ifirmware/common/include -Ifirmware/common/src \
  tests/guiwire_loopback.c firmware/common/src/*.c \
  firmware/common/src/lib/server/*.c \
  firmware/common/src/lib/objects/*.c \
  firmware/common/src/lib/renderer/*.c \
  -lm -o /tmp/guiwire_loopback
/tmp/guiwire_loopback
```

The test exchanges `PING`, `POLL`, the initial scene commands and `NO_DATA`
and verifies that the clock text and three pie charts arrive without a
session reset. It checks the portable protocol, not the RP2040 UART pin mux,
the discrete transceiver, UART byte timing or the physical display.
