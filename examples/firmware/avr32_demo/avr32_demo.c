// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// ATmega32 at the factory clock (internal RC, 1 MHz):
// - JTAG off (JTD written twice), so PC2-PC5 count every 250 ms;
// - the potentiometer on ADC0 (PA0) sets Timer0's fast PWM on OC0 (PB3);
// - INT2 (PB2, a button to GND with the pull-up on) toggles the LED on PD7
//   on each press;
// - every 500 ms the USART (9600 baud, U2X) prints the potentiometer's code
//   and the differential reading ADC1 - ADC0 with a gain of 10.
#include <avr/interrupt.h>
#include <avr/io.h>
#include <util/delay.h>

ISR(INT2_vect)
{
    PORTD ^= _BV(PD7);
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

static void print_number(int n)
{
    char digits[6];
    int i = 0;
    if (n < 0) {
        put('-');
        n = -n;
    }
    do {
        digits[i++] = (char)('0' + n % 10);
        n /= 10;
    } while (n);
    while (i) {
        put(digits[--i]);
    }
}

static int convert(unsigned char mux)
{
    ADMUX = (unsigned char)(_BV(REFS0) | mux); // AVCC
    ADCSRA = _BV(ADEN) | _BV(ADSC) | _BV(ADPS1) | _BV(ADPS0); // 1 MHz / 8
    while (ADCSRA & _BV(ADSC)) {
    }
    return (int)ADC;
}

int main(void)
{
    MCUCSR = _BV(JTD); // JTAG off: two writes within four cycles
    MCUCSR = _BV(JTD);
    DDRC = 0x3C;
    DDRD = _BV(PD7);
    DDRB = _BV(PB3);
    PORTB = _BV(PB2); // the button's pull-up
    TCCR0 = _BV(WGM00) | _BV(WGM01) | _BV(COM01) | _BV(CS01); // fast PWM, /8
    MCUCSR &= (unsigned char)~_BV(ISC2); // INT2 on a falling edge
    GICR = _BV(INT2);
    UCSRA = _BV(U2X);
    UBRRH = 0;
    UBRRL = 12;
    UCSRC = _BV(URSEL) | _BV(UCSZ1) | _BV(UCSZ0);
    UCSRB = _BV(TXEN);
    sei();
    unsigned char count = 0;
    for (;;) {
        const int pot = convert(0);
        OCR0 = (unsigned char)(pot >> 2);
        int diff = convert(9); // ADC1 - ADC0, gain 10: a 10-bit two's complement code
        if (diff & 0x200) {
            diff -= 1024;
        }
        if (count % 2 == 0) {
            print("pot ");
            print_number(pot);
            print(" diff ");
            print_number(diff);
            print("\r\n");
        }
        PORTC = (unsigned char)((PORTC & ~0x3C) | ((count & 0x0F) << 2));
        ++count;
        _delay_ms(250);
    }
}
