# ATmega8: avr_button

A button from PD2 (INT0) to ground, with the internal pull-up, toggles the LED on PB0 on each press. INT0 catches the falling edge; a 1 ms tick from Timer1 in CTC mode ignores bounces for 30 ms. PD4..PD7 count the presses. Factory clock (1 MHz).

Built for the ATmega8 (and ATmega8A) with avr-gcc 7.3.0 and avr-libc 2.0.0 by
`tools/build_avr_firmware.py`, which records the compiler and the SHA-256 of
each file in `examples/firmware/avr_firmware.json`. Run it without the circuit:

```text
simnodus-emu --avr atmega8 examples/firmware/avr_button/avr_button.elf --ms 2000
```

License: MIT (this source). The `.elf` and `.hex` files also contain avr-libc's
start-up code and library functions (BSD-3-Clause, Copyright the avr-libc
developers).
