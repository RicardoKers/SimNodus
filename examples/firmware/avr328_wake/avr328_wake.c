// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// ATmega328P at the factory clock (1 MHz), asleep in power-down almost all
// the time:
// - the watchdog's interrupt wakes it every second (WDP = 0110) and it
//   toggles the LED on PB5;
// - a button from PD4 to GND (pull-up on) wakes it through the pin change
//   interrupt PCINT20 and it toggles the LED on PB4 on each press;
// - INT0 (PD2) on a low level wakes it too and lights PB3 while it is low.
// The brown-out detector is turned off during sleep (BODS), as low-power
// firmware does.
#include <avr/interrupt.h>
#include <avr/io.h>
#include <avr/sleep.h>
#include <avr/wdt.h>

static volatile unsigned char ticks;

ISR(WDT_vect)
{
    ++ticks;
    PINB = _BV(PB5);
}

ISR(PCINT2_vect)
{
    if (!(PIND & _BV(PD4))) {
        PINB = _BV(PB4); // pressed
    }
}

ISR(INT0_vect)
{
    PORTB |= _BV(PB3);
    EIMSK &= (unsigned char)~_BV(INT0); // until the pin is high again
}

int main(void)
{
    MCUSR = 0;
    DDRB = _BV(PB5) | _BV(PB4) | _BV(PB3);
    PORTD = _BV(PD4) | _BV(PD2);
    PCMSK2 = _BV(PCINT20);
    PCICR = _BV(PCIE2);
    EICRA = 0; // INT0 on a low level
    EIMSK = _BV(INT0);
    wdt_reset();
    WDTCSR = _BV(WDCE) | _BV(WDE);
    WDTCSR = _BV(WDIE) | _BV(WDP2) | _BV(WDP1); // interrupt only, 1 s
    set_sleep_mode(SLEEP_MODE_PWR_DOWN);
    sei();
    for (;;) {
        if ((PIND & _BV(PD2)) && !(EIMSK & _BV(INT0))) {
            PORTB &= (unsigned char)~_BV(PB3);
            EIMSK |= _BV(INT0);
        }
        cli();
        sleep_enable();
        sleep_bod_disable();
        sei();
        sleep_cpu();
        sleep_disable();
    }
}
