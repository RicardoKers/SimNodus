// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// ATmega8 at the factory clock (1 MHz): the USART at 9600 baud, 8N1, with
// U2X (UBRR = 12, 0.2 % error; without U2X 1 MHz gives 7 % at 9600). It
// prints a greeting, then echoes what it receives through the RX interrupt;
// "l" toggles the LED on PB0 and "?" prints how many bytes arrived. The
// counter lives in the EEPROM at address 0, so it survives a reset.
#include <avr/eeprom.h>
#include <avr/interrupt.h>
#include <avr/io.h>

static volatile unsigned char received[16];
static volatile unsigned char head, tail;

ISR(USART_RXC_vect)
{
    const unsigned char c = UDR;
    const unsigned char next = (unsigned char)((head + 1) & 15);
    if (next != tail) {
        received[head] = c;
        head = next;
    }
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
    UBRRH = 0;
    UBRRL = 12;
    UCSRC = _BV(URSEL) | _BV(UCSZ1) | _BV(UCSZ0);
    UCSRB = _BV(RXEN) | _BV(TXEN) | _BV(RXCIE);
    unsigned int count = eeprom_read_word((uint16_t*)0);
    if (count == 0xFFFF) {
        count = 0;
    }
    sei();
    print("ATmega8 ready\r\n> ");
    for (;;) {
        if (head == tail) {
            continue;
        }
        const unsigned char c = received[tail];
        tail = (unsigned char)((tail + 1) & 15);
        ++count;
        eeprom_update_word((uint16_t*)0, count);
        if (c == 'l') {
            PORTB ^= _BV(PB0);
        } else if (c == '?') {
            print("\r\nbytes: ");
            print_number(count);
            print("\r\n> ");
            continue;
        }
        put((char)c);
    }
}
