# ATmega8: avr_sleep

A small real-time clock at the factory clock (internal RC, 1 MHz): Timer2 runs from a 32.768 kHz watch crystal on TOSC1/TOSC2 (PB6, PB7; AS2), prescaler 128, and overflows once a second. Between the overflows the chip sleeps in Power-save. Each second the LED on PB0 toggles and the USART (9600 baud, U2X) prints `seconds 12`. Example 54 runs it.

```text
simnodus-emu --avr atmega8a examples/firmware/avr_sleep/avr_sleep.elf --crystal 32768 --ms 3500
```

Built for the ATmega8 (and ATmega8A) with avr-gcc 7.3.0 and avr-libc 2.0.0 by
`tools/build_avr_firmware.py`, which records the compiler and the SHA-256 of
each file in `examples/firmware/avr_firmware.json`.

License: MIT (this source). The `.elf` and `.hex` files also contain avr-libc's
start-up code and library functions (BSD-3-Clause, Copyright the avr-libc
developers).
