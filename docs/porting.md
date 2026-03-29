# Porting Notes

The project is intentionally split into:
- protocol/core
- client and server logic
- renderer and display driver
- platform adapter

A new platform should implement:
- controller-side I2C write
- SPI write
- GPIO output
- millisecond delay

The GUI2C core itself should remain free of MCU SDK includes.
