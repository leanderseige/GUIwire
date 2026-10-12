# GUIwire protocol loopback

From the repository root, compile and run the portable half-duplex
request/response test with the ATmega demonstrator's packet and queue sizes:

```sh
cc -std=c11 -O2 -DGUIWIRE_DEVICE_QUEUE_CAPACITY=9u \
  -DGUIWIRE_MAX_PACKET_SIZE=98u \
  -Ifirmware/common/include -Ifirmware/common/src \
  tests/guiwire_loopback.c firmware/common/src/*.c \
  firmware/common/src/lib/server/*.c \
  firmware/common/src/lib/objects/*.c \
  firmware/common/src/lib/renderer/*.c \
  -lm -o /tmp/guiwire_loopback
/tmp/guiwire_loopback
```

The compatibility test exchanges legacy `PING`, `POLL`, the initial scene
commands and `NO_DATA`
and verifies that the clock text and three pie charts arrive without a
session reset. It checks the portable protocol, not the RP2040 UART pin mux,
the discrete transceiver, UART byte timing or the physical display.

The adaptive-rate state-machine test uses the same portable sources:

```sh
cc -std=c11 -O2 -DGUIWIRE_DEVICE_QUEUE_CAPACITY=9u \
  -DGUIWIRE_MAX_PACKET_SIZE=98u \
  -Ifirmware/common/include -Ifirmware/common/src \
  tests/guiwire_adaptive_loopback.c firmware/common/src/*.c \
  firmware/common/src/lib/server/*.c \
  firmware/common/src/lib/objects/*.c \
  firmware/common/src/lib/renderer/*.c \
  -lm -o /tmp/guiwire_adaptive_loopback
/tmp/guiwire_adaptive_loopback
```

It uses reliable sessions and keeps ring updates flowing while testing successful upshifts, a failed
rate, lost acknowledgements on up- and downshifts, downshifts after a missing
response or a CRC error, a fixed-rate Device that remains at 9600 baud, and a retry after the
30-second cooldown. The simulated bus enforces matching baud settings but
cannot establish that the
discrete transceiver passes a particular bitrate on real hardware.

The hot-patch test exercises delayed cable insertion, lost and CRC-corrupt
GUI responses, duplicate suppression, local debug-view caching and a switch
to an already-active second client while debug is held,
unplug/replug and a high-rate-to-base-rate client switch:

```sh
cc -std=c11 -O2 -DGUIWIRE_DEVICE_QUEUE_CAPACITY=9u \
  -DGUIWIRE_MAX_PACKET_SIZE=98u \
  -Ifirmware/common/include -Ifirmware/common/src \
  tests/guiwire_hotplug_loopback.c firmware/common/src/*.c \
  firmware/common/src/lib/server/*.c \
  firmware/common/src/lib/objects/*.c \
  firmware/common/src/lib/renderer/*.c \
  -lm -o /tmp/guiwire_hotplug_loopback
/tmp/guiwire_hotplug_loopback
```

These tests model software and exact bytes, not physical patch contacts,
display-rendering latency or MCU UART timing. Those still require the two
firmwares to be tested together on the real demonstrator.
