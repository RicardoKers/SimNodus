# ATmega8: avr_blink

An LED on PB0 toggles every 500 ms with avr-libc's `_delay_ms`. `avr_blink.elf` runs at the factory clock (internal RC, 1 MHz); `avr_blink_16mhz.elf` carries the fuses for a 16 MHz crystal (low 0xEF, high 0xC9: CKSEL 1111, SUT 10, CKOPT) and needs a crystal on XTAL1/XTAL2. The `.hex` files hold the same flash without the fuses.

Built for the ATmega8 (and ATmega8A) with avr-gcc 7.3.0 and avr-libc 2.0.0 by
`tools/build_avr_firmware.py`, which records the compiler and the SHA-256 of
each file in `examples/firmware/avr_firmware.json`. Run it without the circuit:

```text
simnodus-emu --avr atmega8 examples/firmware/avr_blink/avr_blink.elf --ms 2000
```

License: MIT (this source). The `.elf` and `.hex` files also contain avr-libc's
start-up code and library functions (BSD-3-Clause, Copyright the avr-libc
developers).
