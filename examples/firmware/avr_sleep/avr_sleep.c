// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// ATmega8 at the factory clock (internal RC, 1 MHz) as a small real-time
// clock: Timer2 runs from a 32.768 kHz watch crystal on TOSC1/TOSC2 (PB6,
// PB7; AS2), prescaler 128, so it overflows once a second. Between the
// overflows the chip sleeps in Power-save, where only Timer2 runs. Each
// second the LED on PB0 toggles and the USART (9600 baud, U2X) prints
// "seconds 12".
#include <avr/interrupt.h>
#include <avr/io.h>
#include <avr/sleep.h>

static volatile unsigned int seconds;

ISR(TIMER2_OVF_vect)
{
    ++seconds;
    PORTB ^= _BV(PB0);
}

static void put(char c)
{
    while (!(UCSRA & _BV(UDRE))) {
    }
    UDR = (unsigned char)c;
}

static void print_number(unsigned int n)
{
    char digits[6];
    int i = 0;
    do {
        digits[i++] = (char)('0' + n % 10);
        n /= 10;
    } while (n);
    while (i) {
        put(digits[--i]);
    }
}

int main(void)
{
    DDRB |= _BV(PB0);
    UCSRA = _BV(U2X);
    UBRRL = 12;
    UCSRC = _BV(URSEL) | _BV(UCSZ1) | _BV(UCSZ0);
    UCSRB = _BV(TXEN);
    ASSR = _BV(AS2); // Timer2 from the watch crystal
    TCNT2 = 0;
    TCCR2 = _BV(CS22) | _BV(CS20); // 32768 / 128 / 256 = 1 Hz
    while (ASSR & (_BV(TCN2UB) | _BV(TCR2UB))) {
    }
    TIFR = _BV(TOV2);
    TIMSK |= _BV(TOIE2);
    set_sleep_mode(SLEEP_MODE_PWR_SAVE);
    sei();
    unsigned int shown = 0;
    for (;;) {
        if (seconds != shown) {
            shown = seconds;
            const char* text = "seconds ";
            while (*text) {
                put(*text++);
            }
            print_number(shown);
            put('\r');
            put('\n');
            UCSRA |= _BV(TXC); // wait for the last frame before the clock stops
            while (!(UCSRA & _BV(TXC))) {
            }
        }
        // As the datasheet asks: a write to TCCR2, then wait, so that the
        // interrupt logic is ready before sleeping again.
        TCCR2 = _BV(CS22) | _BV(CS20);
        while (ASSR & _BV(TCR2UB)) {
        }
        sleep_mode();
    }
}
