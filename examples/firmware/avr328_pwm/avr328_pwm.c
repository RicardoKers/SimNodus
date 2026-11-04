// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// ATmega328P at the factory clock (1 MHz): the four 8-bit PWM outputs and an
// ADC that converts on its own.
// - Timer0, fast PWM, prescaler 8: OC0A (PD6) high a quarter of the period,
//   OC0B (PD5) inverting, low a quarter.
// - Timer2, phase-correct PWM, prescaler 8: OC2A (PB3) follows the
//   potentiometer on ADC0 (PC0); OC2B (PD3) is the opposite duty.
// - The ADC is auto-triggered by Timer0's overflow (ADTS = 100) and its
//   interrupt writes the result to OCR2A and OCR2B.
// - Timer1 in CTC mode toggles OC1A (PB1) every 10 ms: a 50 Hz square wave.
#include <avr/interrupt.h>
#include <avr/io.h>

ISR(ADC_vect)
{
    const unsigned char value = ADCH; // left adjusted: the top 8 bits
    OCR2A = value;
    OCR2B = (unsigned char)(255 - value);
    TIFR0 = _BV(TOV0); // the next overflow is a new trigger
}

int main(void)
{
    DDRD = _BV(PD6) | _BV(PD5) | _BV(PD3);
    DDRB = _BV(PB3) | _BV(PB1);
    DIDR0 = _BV(ADC0D);

    OCR0A = 63;
    OCR0B = 191;
    TCCR0A = _BV(COM0A1) | _BV(COM0B1) | _BV(COM0B0) | _BV(WGM01) | _BV(WGM00);
    TCCR0B = _BV(CS01);

    TCCR2A = _BV(COM2A1) | _BV(COM2B1) | _BV(WGM20);
    TCCR2B = _BV(CS21);

    OCR1A = 9999;
    TCCR1A = _BV(COM1A0);
    TCCR1B = _BV(WGM12) | _BV(CS10);

    ADMUX = _BV(REFS0) | _BV(ADLAR); // AVCC, ADC0, left adjusted
    ADCSRB = _BV(ADTS2);             // Timer0 overflow
    ADCSRA = _BV(ADEN) | _BV(ADATE) | _BV(ADIE) | _BV(ADPS1); // 1 MHz / 4
    sei();
    for (;;) {
    }
}
