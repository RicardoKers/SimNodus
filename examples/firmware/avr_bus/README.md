# ATmega8: avr_bus

TWI and SPI with chips, at 8 MHz (internal RC; its ELF sets the low fuse E4). TWI at 100 kHz (TWBR = 32) reads an LM75 at 0x48 and writes a byte to a 24C02 at 0x50, then reads it back; SPI at 2 MHz (F_CPU / 4) shifts a counter into a 74HC595 (STCP on PB2). Every 500 ms the USART (9600 baud, U2X, UBRR = 103) reports `LM75 25.0 C 24C02 5A 595 05`. Example 53 runs it.

Built for the ATmega8 (and ATmega8A) with avr-gcc 7.3.0 and avr-libc 2.0.0 by
`tools/build_avr_firmware.py`, which records the compiler and the SHA-256 of
each file in `examples/firmware/avr_firmware.json`.

License: MIT (this source). The `.elf` and `.hex` files also contain avr-libc's
start-up code and library functions (BSD-3-Clause, Copyright the avr-libc
developers).
