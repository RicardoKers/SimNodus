// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// ATmega8 at 8 MHz (internal RC; low fuse E4): TWI and SPI with chips.
//   TWI at 100 kHz (TWBR = 32): the LM75 at 0x48 (its temperature), the
//   24C02 at 0x50 (a byte written at address 0x10, then read back).
//   SPI master at 2 MHz (F_CPU / 4) to a 74HC595: DS on MOSI (PB3), SHCP on
//   SCK (PB5), STCP on PB2 (SS, an output so that the SPI stays master);
//   its outputs count up every 100 ms.
// Every 500 ms the USART (9600 baud, U2X, UBRR = 103) reports:
//   "LM75 25.0 C 24C02 5A 595 07"
#include <avr/io.h>
#include <util/delay.h>
#include <util/twi.h>

#if F_CPU == 8000000UL
FUSES = {.low = 0xE4, .high = HFUSE_DEFAULT};
#endif

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

static void print_hex(unsigned char v)
{
    static const char digits[] = "0123456789ABCDEF";
    put(digits[v >> 4]);
    put(digits[v & 15]);
}

static void print_number(int n)
{
    if (n < 0) {
        put('-');
        n = -n;
    }
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

// TWI steps: each waits for TWINT and returns the status.
static unsigned char twi(unsigned char control)
{
    TWCR = control | _BV(TWINT) | _BV(TWEN);
    if (control & _BV(TWSTO)) {
        while (TWCR & _BV(TWSTO)) {
        }
        return 0xF8;
    }
    while (!(TWCR & _BV(TWINT))) {
    }
    return TW_STATUS;
}

static unsigned char twi_write(unsigned char byte)
{
    TWDR = byte;
    return twi(0);
}

static int lm75_half_degrees(void)
{
    twi(_BV(TWSTA));
    if (twi_write((0x48 << 1) | TW_READ) != TW_MR_SLA_ACK) {
        twi(_BV(TWSTO));
        return -999;
    }
    twi(_BV(TWEA));
    const unsigned char msb = TWDR;
    twi(0); // NACK the last byte
    const unsigned char lsb = TWDR;
    twi(_BV(TWSTO));
    return (int)(signed char)msb * 2 + (lsb >> 7);
}

static void eeprom_write(unsigned char address, unsigned char value)
{
    twi(_BV(TWSTA));
    twi_write((0x50 << 1) | TW_WRITE);
    twi_write(address);
    twi_write(value);
    twi(_BV(TWSTO));
    _delay_ms(6); // the 24C02's write cycle (5 ms)
}

static unsigned char eeprom_read(unsigned char address)
{
    twi(_BV(TWSTA));
    twi_write((0x50 << 1) | TW_WRITE);
    twi_write(address);
    twi(_BV(TWSTA)); // repeated START
    twi_write((0x50 << 1) | TW_READ);
    twi(0); // one byte, NACK
    const unsigned char value = TWDR;
    twi(_BV(TWSTO));
    return value;
}

static void shift_out(unsigned char value)
{
    SPDR = value;
    while (!(SPSR & _BV(SPIF))) {
    }
    PORTB |= _BV(PB2); // STCP: latch
    PORTB &= ~_BV(PB2);
}

int main(void)
{
    UCSRA = _BV(U2X);
    UBRRL = 103;
    UCSRC = _BV(URSEL) | _BV(UCSZ1) | _BV(UCSZ0);
    UCSRB = _BV(TXEN);
    TWBR = 32; // 8 MHz / (16 + 64) = 100 kHz
    DDRB |= _BV(PB2) | _BV(PB3) | _BV(PB5);
    SPCR = _BV(SPE) | _BV(MSTR); // F_CPU / 4, mode 0, MSB first
    eeprom_write(0x10, 0x5A);
    unsigned char count = 0;
    for (;;) {
        for (int i = 0; i < 5; ++i) {
            shift_out(++count);
            _delay_ms(100);
        }
        const int t = lm75_half_degrees();
        print("LM75 ");
        print_number(t / 2);
        print(t & 1 ? ".5" : ".0");
        print(" C 24C02 ");
        print_hex(eeprom_read(0x10));
        print(" 595 ");
        print_hex(count);
        print("\r\n");
    }
}
