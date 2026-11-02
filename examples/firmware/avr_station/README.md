# ATmega8: avr_station

A student project in small, at the factory clock (1 MHz): a 1 ms tick from Timer1 (CTC) keeps everything in step. Every 10 ms it reads the potentiometer on ADC0 and sets Timer2's fast PWM on OC2 (PB3, 3.9 kHz) to it; every 500 ms it toggles the LED on PB0; every 100 ms it reports `t 1200 pot 512 pwm 128` over the USART (9600 baud, U2X). Example 52 runs it.

Built for the ATmega8 (and ATmega8A) with avr-gcc 7.3.0 and avr-libc 2.0.0 by
`tools/build_avr_firmware.py`, which records the compiler and the SHA-256 of
each file in `examples/firmware/avr_firmware.json`. Run it without the circuit:

```text
simnodus-emu --avr atmega8 examples/firmware/avr_station/avr_station.elf --ms 2000
```

License: MIT (this source). The `.elf` and `.hex` files also contain avr-libc's
start-up code and library functions (BSD-3-Clause, Copyright the avr-libc
developers).
