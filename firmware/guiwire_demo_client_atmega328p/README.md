# GUIwire ATmega328P Demo Client

This is a GUIwire Device demo for an ATmega328P. It displays elapsed
`MM:SS.hh` time in the upper half. The three lower pie charts show ADC0,
ADC1 and the current second in hundredths. The first two charts represent
the potentiometer voltage from 0 to 100 percent; the third chart represents
0 to 99 hundredths of the current second.

## Wiring and operation

The AZ-Delivery Nano uses its hardware UART for GUIwire:

```text
ATmega328P D1 / TX  -> 5 V side of the GUIwire transceiver TX input
ATmega328P D0 / RX  <- 5 V side of the GUIwire transceiver RX output
ATmega328P 5 V      -> transceiver 5 V / MCU-VIO
ATmega328P GND      -> transceiver GND
```

Connect the two potentiometers as voltage dividers between the local 5 V and
GND rails:

```text
ADC0 / A0 (pin 14) <- wiper of potentiometer 0
ADC1 / A1 (pin 15) <- wiper of potentiometer 1
```

The ADC uses AVcc as its reference. Do not apply more than the local 5 V rail
to either analog input. The values are shown below the circles as `A0: xx%`
and `A1: xx%`; the third label is `1/100: xx%`.

Connect the transceiver's protected `DATA` and `GND` to the 3.5 mm TS
GUIwire patch cable. On the RP2040 Host side, use the 3.3 V transceiver and
its UART pins TX=GPIO0 and RX=GPIO1. Do not connect the ATmega UART pins
directly to the RP2040 or to the patch cable.

After uploading, disconnect the Nano's USB cable for normal operation, or do
not open a serial terminal on it: the CH340 is connected to the same D0/D1
UART used by GUIwire. Power the Nano and its transceiver locally.

## Compile

The default build expects `avr-gcc`, `avr-objcopy` and `avr-size`:

```sh
make -B -C firmware/guiwire_demo_client_atmega328p BAUD=9600 NEGOTIATE=0
make -C firmware/guiwire_demo_client_atmega328p size
```

The demonstrator uses a fixed 9600-baud GUIwire link by default. The Host and
client must be built with the same `BAUD` value. Automatic baud negotiation
can be enabled explicitly with `NEGOTIATE=1` on the client and the matching
option on the Host.

The default output is:

```text
firmware/guiwire_demo_client_atmega328p/build/guiwire_demo_client_atmega328p.hex
```

Override MCU, clock or build directory when required:

```sh
make -C firmware/guiwire_demo_client_atmega328p \
  MCU=atmega328p F_CPU=16000000UL BUILD_DIR=/tmp/guiwire-avr-build
```

## Upload with avrdude

The AZ-Delivery Nano V3 with CH340 normally uses the Arduino Nano old
bootloader. Its upload baud rate is therefore `57600`; this is separate from
the GUIwire UART, which stays at 9600 baud in the default demonstrator build.

For the AZ-Delivery board, replace the port with the CH340 serial device shown
by your operating system:

```sh
make -C firmware/guiwire_demo_client_atmega328p flash \
  PROGRAMMER=arduino PORT=/dev/cu.wchusbserialXXXX
```

On Linux the CH340 port is often `/dev/ttyUSB0`. If the board has a newer
115200-baud bootloader, override only the upload speed:

```sh
make -C firmware/guiwire_demo_client_atmega328p flash \
  PROGRAMMER=arduino PORT=/dev/ttyUSB0 UPLOAD_BAUD=115200
```

For a USBasp or another programmer, select its avrdude programmer name with
`PROGRAMMER` and provide the appropriate `PORT` value if required:

```sh
make -C firmware/guiwire_demo_client_atmega328p flash \
  PROGRAMMER=usbasp PORT=/dev/cu.usbserialXXXX
```

The application uses 8N1 on its GUIwire transceiver UART. The CH340 driver
must be installed if the serial port does not appear. The CH340 USB UART is
not used for GUIwire traffic while the application is running.
