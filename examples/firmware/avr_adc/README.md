# ATmega8: avr_adc

The ADC at the factory clock (1 MHz), 125 kHz ADC clock. Every 100 ms it converts ADC0 (PC0) and ADC1 (PC1) against AVCC, ADC0 against the internal 2.56 V reference and the 1.30 V bandgap, and prints the four codes over the USART (9600 baud, U2X): `A0 512 A1 300 A0/2.56 1000 BG 266`.

Built for the ATmega8 (and ATmega8A) with avr-gcc 7.3.0 and avr-libc 2.0.0 by
`tools/build_avr_firmware.py`, which records the compiler and the SHA-256 of
each file in `examples/firmware/avr_firmware.json`. Run it without the circuit:

```text
simnodus-emu --avr atmega8 examples/firmware/avr_adc/avr_adc.elf --ms 2000
```

License: MIT (this source). The `.elf` and `.hex` files also contain avr-libc's
start-up code and library functions (BSD-3-Clause, Copyright the avr-libc
developers).
