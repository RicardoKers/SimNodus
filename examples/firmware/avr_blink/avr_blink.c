// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// ATmega8: an LED on PB0 toggles every 500 ms with _delay_ms. Built twice
// (tools/build_avr_firmware.py): at the factory clock (internal RC, 1 MHz)
// and at 16 MHz with a crystal, each with its fuses in the ELF.
#include <avr/io.h> // before <avr/fuse.h>, which needs the chip's definitions
#include <avr/fuse.h>
#include <util/delay.h>

#if F_CPU == 16000000UL
// Crystal 3-16 MHz with CKOPT, 16K CK + 4.1 ms start-up (CKSEL 1111, SUT 10).
FUSES = {.low = 0xEF, .high = 0xC9};
#else
FUSES = {.low = LFUSE_DEFAULT, .high = HFUSE_DEFAULT}; // internal RC 1 MHz
#endif

int main(void)
{
    DDRB |= _BV(PB0);
    for (;;) {
        PORTB ^= _BV(PB0);
        _delay_ms(500);
    }
}
