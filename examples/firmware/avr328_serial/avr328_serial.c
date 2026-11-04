// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// ATmega328P from its factory fuses (the internal 8 MHz RC divided by 8 by
// CKDIV8): the firmware sets CLKPR to divide by 1 and runs at 8 MHz. USART0
// at 38400 baud, 8N1, with U2X (UBRR0 = 25, 0.2 % error). It prints a
// greeting, then echoes what it receives through the RX interrupt:
//   "l" toggles the LED on PB5 (writing 1 to PINB toggles PORTB),
//   "?" prints how many bytes arrived (a counter in the EEPROM, so it
//       survives a reset),
//   "t" prints the temperature sensor's ADC code (1.1 V reference).
#include <avr/eeprom.h>
#include <avr/interrupt.h>
#include <avr/io.h>
#include <avr/power.h>

static volatile unsigned char received[16];
static volatile unsigned char head, tail;

ISR(USART_RX_vect)
{
    const unsigned char c = UDR0;
    const unsigned char next = (unsigned char)((head + 1) & 15);
    if (next != tail) {
        received[head] = c;
        head = next;
    }
}

static void put(char c)
{
    while (!(UCSR0A & _BV(UDRE0))) {
    }
    UDR0 = (unsigned char)c;
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

static unsigned int temperature(void)
{
    ADMUX = _BV(REFS1) | _BV(REFS0) | 8; // the 1.1 V reference, ADC8
    ADCSRA = _BV(ADEN) | _BV(ADSC) | _BV(ADPS2) | _BV(ADPS1); // 8 MHz / 64
    while (ADCSRA & _BV(ADSC)) {
    }
    return ADC;
}

int main(void)
{
    clock_prescale_set(clock_div_1);
    DDRB |= _BV(PB5);
    UCSR0A = _BV(U2X0);
    UBRR0 = 25;
    UCSR0C = _BV(UCSZ01) | _BV(UCSZ00);
    UCSR0B = _BV(RXEN0) | _BV(TXEN0) | _BV(RXCIE0);
    unsigned int count = eeprom_read_word((uint16_t*)0);
    if (count == 0xFFFF) {
        count = 0;
    }
    sei();
    print("ATmega328P ready\r\n> ");
    for (;;) {
        if (head == tail) {
            continue;
        }
        const unsigned char c = received[tail];
        tail = (unsigned char)((tail + 1) & 15);
        ++count;
        eeprom_update_word((uint16_t*)0, count);
        if (c == 'l') {
            PINB = _BV(PB5);
        } else if (c == '?') {
            print("\r\nbytes: ");
            print_number(count);
            print("\r\n> ");
        } else if (c == 't') {
            print("\r\ntemperature: ");
            print_number(temperature());
            print("\r\n> ");
        } else {
            put((char)c);
        }
    }
}
