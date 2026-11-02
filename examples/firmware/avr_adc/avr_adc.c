// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// ATmega8 at the factory clock (1 MHz): the ADC with a 125 kHz ADC clock
// (prescaler 8). Every 100 ms it converts ADC0 (PC0) and ADC1 (PC1) against
// AVCC, ADC0 against the internal 2.56 V reference, and the 1.30 V bandgap,
// and prints the four codes over the USART (9600 baud, 8N1, U2X):
//   "A0 512 A1 300 A0/2.56 1000 BG 266"
#include <avr/io.h>
#include <util/delay.h>

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

static unsigned int convert(unsigned char admux)
{
    ADMUX = admux;
    ADCSRA |= _BV(ADSC);
    while (ADCSRA & _BV(ADSC)) {
    }
    return ADC; // ADCL, then ADCH
}

int main(void)
{
    UCSRA = _BV(U2X);
    UBRRL = 12;
    UCSRC = _BV(URSEL) | _BV(UCSZ1) | _BV(UCSZ0);
    UCSRB = _BV(TXEN);
    ADCSRA = _BV(ADEN) | _BV(ADPS1) | _BV(ADPS0); // 1 MHz / 8
    for (;;) {
        const unsigned int a0 = convert(_BV(REFS0) | 0);
        const unsigned int a1 = convert(_BV(REFS0) | 1);
        const unsigned int a0i = convert(_BV(REFS1) | _BV(REFS0) | 0);
        const unsigned int bg = convert(_BV(REFS0) | 14);
        print("A0 ");
        print_number(a0);
        print(" A1 ");
        print_number(a1);
        print(" A0/2.56 ");
        print_number(a0i);
        print(" BG ");
        print_number(bg);
        print("\r\n");
        _delay_ms(100);
    }
}
