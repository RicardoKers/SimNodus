// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// ATmega8 at the factory clock (1 MHz): a button from PD2 (INT0) to ground,
// with the internal pull-up, toggles the LED on PB0. INT0 catches the falling
// edge; a 1 ms tick from Timer1 in CTC mode ignores bounces for 30 ms. The
// number of presses is shown on PD4..PD7 as a 4-bit counter.
#include <avr/interrupt.h>
#include <avr/io.h>

static volatile unsigned int ms;
static volatile unsigned int last_press;
static volatile unsigned char presses;

ISR(TIMER1_COMPA_vect)
{
    ++ms;
}

ISR(INT0_vect)
{
    const unsigned int now = ms;
    if ((unsigned int)(now - last_press) < 30) {
        return; // a bounce
    }
    last_press = now;
    PORTB ^= _BV(PB0);
    ++presses;
    PORTD = (PORTD & 0x0F) | (unsigned char)(presses << 4);
}

int main(void)
{
    DDRB |= _BV(PB0);
    DDRD |= 0xF0;
    PORTD |= _BV(PD2); // pull-up on the button's pin
    // Timer1 CTC (mode 4): 1 MHz / 1 / (999 + 1) = 1 kHz.
    OCR1A = 999;
    TCCR1B = _BV(WGM12) | _BV(CS10);
    TIMSK |= _BV(OCIE1A);
    // INT0 on the falling edge.
    MCUCR = (MCUCR & ~(_BV(ISC00) | _BV(ISC01))) | _BV(ISC01);
    GICR |= _BV(INT0);
    last_press = (unsigned int)-1000;
    sei();
    for (;;) {
    }
}
