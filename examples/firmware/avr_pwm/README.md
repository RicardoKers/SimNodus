# ATmega8: avr_pwm

The three timers' waveforms at the factory clock (1 MHz): Timer1 fast PWM with ICR1 as TOP, 500 Hz, OC1A (PB1) non-inverting and OC1B (PB2) inverting, both 25 %; Timer2 phase-correct PWM on OC2 (PB3), 245.1 Hz, 25.1 %; a Timer0 overflow interrupt toggling PD7 every 16.384 ms.

Built for the ATmega8 (and ATmega8A) with avr-gcc 7.3.0 and avr-libc 2.0.0 by
`tools/build_avr_firmware.py`, which records the compiler and the SHA-256 of
each file in `examples/firmware/avr_firmware.json`. Run it without the circuit:

```text
simnodus-emu --avr atmega8 examples/firmware/avr_pwm/avr_pwm.elf --ms 2000
```

License: MIT (this source). The `.elf` and `.hex` files also contain avr-libc's
start-up code and library functions (BSD-3-Clause, Copyright the avr-libc
developers).
