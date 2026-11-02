// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// ATmega8 at the factory clock (1 MHz): the three timers' waveforms.
//   Timer1, fast PWM with ICR1 as TOP (mode 14), prescaler 1:
//     500 Hz = 1 MHz / (1999 + 1); OC1A (PB1) non-inverting, OCR1A = 499
//     (25 %); OC1B (PB2) inverting, OCR1B = 1499 (25 %).
//   Timer2, phase-correct PWM, prescaler 8: 1 MHz / (8 * 510) = 245.1 Hz on
//     OC2 (PB3), OCR2 = 64 (64 / 255 = 25.1 %).
//   Timer0 overflow, prescaler 64: every 16.384 ms an interrupt toggles PD7
//     (a 32.768 ms period).
#include <avr/interrupt.h>
#include <avr/io.h>

ISR(TIMER0_OVF_vect)
{
    PORTD ^= _BV(PD7);
}

int main(void)
{
    DDRB |= _BV(PB1) | _BV(PB2) | _BV(PB3);
    DDRD |= _BV(PD7);

    ICR1 = 1999;
    OCR1A = 499;
    OCR1B = 1499;
    TCCR1A = _BV(COM1A1) | _BV(COM1B1) | _BV(COM1B0) | _BV(WGM11);
    TCCR1B = _BV(WGM13) | _BV(WGM12) | _BV(CS10);

    OCR2 = 64;
    TCCR2 = _BV(WGM20) | _BV(COM21) | _BV(CS21);

    TCCR0 = _BV(CS01) | _BV(CS00);
    TIMSK |= _BV(TOIE0);
    sei();
    for (;;) {
    }
}
