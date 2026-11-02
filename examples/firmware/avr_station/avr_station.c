// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// ATmega8 at the factory clock (1 MHz), the model of a student project: a
// 1 ms tick from Timer1 (CTC) keeps everything in step.
//   - every 10 ms: read the potentiometer on ADC0 (PC0, AVCC reference) and
//     set the PWM on OC2 (PB3; Timer2 fast PWM, 1 MHz / 256 = 3.9 kHz) to it;
//   - every 500 ms: toggle the status LED on PB0;
//   - every 100 ms: report over the USART (9600 baud, 8N1, U2X):
//     "t 1200 pot 512 pwm 128".
#include <avr/interrupt.h>
#include <avr/io.h>

static volatile unsigned long ms;

ISR(TIMER1_COMPA_vect)
{
    ++ms;
}

static void put(char c)
{
    while (!(UCSRA & _BV(UDRE))) {
    }
    UDR = (unsigned char)c;
}

static void print(const char* text)
{
    while (*text) {
        put(*text++);
    }
}

static void print_number(unsigned long n)
{
    char digits[11];
    int i = 0;
    do {
        digits[i++] = (char)('0' + n % 10);
        n /= 10;
    } while (n);
    while (i) {
        put(digits[--i]);
    }
}

static unsigned long now(void)
{
    cli();
    const unsigned long t = ms;
    sei();
    return t;
}

int main(void)
{
    DDRB |= _BV(PB0) | _BV(PB3);
    UCSRA = _BV(U2X);
    UBRRL = 12;
    UCSRC = _BV(URSEL) | _BV(UCSZ1) | _BV(UCSZ0);
    UCSRB = _BV(TXEN);
    ADMUX = _BV(REFS0);                           // ADC0 against AVCC
    ADCSRA = _BV(ADEN) | _BV(ADPS1) | _BV(ADPS0); // 125 kHz
    TCCR2 = _BV(WGM21) | _BV(WGM20) | _BV(COM21) | _BV(CS20); // fast PWM, non-inverting
    OCR1A = 999;                                  // 1 kHz
    TCCR1B = _BV(WGM12) | _BV(CS10);
    TIMSK |= _BV(OCIE1A);
    sei();
    unsigned long next_read = 0, next_led = 0, next_report = 0;
    unsigned int pot = 0;
    for (;;) {
        const unsigned long t = now();
        if (t >= next_read) {
            next_read += 10;
            ADCSRA |= _BV(ADSC);
            while (ADCSRA & _BV(ADSC)) {
            }
            pot = ADC;
            OCR2 = (unsigned char)(pot >> 2);
        }
        if (t >= next_led) {
            next_led += 500;
            PORTB ^= _BV(PB0);
        }
        if (t >= next_report) {
            next_report += 100;
            print("t ");
            print_number(t);
            print(" pot ");
            print_number(pot);
            print(" pwm ");
            print_number(pot >> 2);
            print("\r\n");
        }
    }
}
