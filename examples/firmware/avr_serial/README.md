# ATmega8: avr_serial

The USART at 9600 baud, 8N1, with U2X (UBRR = 12) at the factory clock (1 MHz). It greets, echoes what it receives through the RX interrupt, toggles the LED on PB0 with `l` and prints with `?` how many bytes arrived. The count lives in the EEPROM (address 0), so it survives a reset and, in a project, closing it.

Built for the ATmega8 (and ATmega8A) with avr-gcc 7.3.0 and avr-libc 2.0.0 by
`tools/build_avr_firmware.py`, which records the compiler and the SHA-256 of
each file in `examples/firmware/avr_firmware.json`. Run it without the circuit:

```text
simnodus-emu --avr atmega8 examples/firmware/avr_serial/avr_serial.elf --ms 2000
```

License: MIT (this source). The `.elf` and `.hex` files also contain avr-libc's
start-up code and library functions (BSD-3-Clause, Copyright the avr-libc
developers).
