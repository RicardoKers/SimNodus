// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// STM32F407 buses (experiment E-12, SN-298), at 16 MHz from the HSI.
//   SPI1 (PA5 SCK, PA7 MOSI) with PA4 as the latch: a 74HC595 shows a
//     binary count, one step every 250 ms.
//   SPI2 (PB13 SCK, PB15 MOSI, 16-bit frames) with PB12 as LOAD: a MAX7219
//     shows a smiley on an 8x8 LED matrix; its last row shows the count.
//   I2C1 (PB6 SCL, PB7 SDA, 100 kHz): writes "SimNodus" into a 24C02 at
//     0x50 (address 0x10), waits for its write cycle by acknowledge polling,
//     reads it back; reads the LM75 at 0x48 every second.
//   USART1 (PA9, 115200 baud) through DMA2 stream 7: "f4_buses",
//     "eeprom SimNodus", then "lm75 23.5" every second.
#define F4_SYSTICK SysTick_Handler
#include "../f4_common/f4_peripherals.h"

#define SPI1_BASE 0x40013000u
#define SPI2_BASE 0x40003800u
#define SPI_CR1(base) REG((base) + 0x00u)
#define SPI_SR(base) REG((base) + 0x08u)
#define SPI_DR(base) REG((base) + 0x0Cu)

#define EEPROM 0x50u
#define LM75 0x48u

static volatile uint32_t milliseconds;

void SysTick_Handler(void)
{
    ++milliseconds;
}

// One frame, then wait until it has left the pins.
static void spi_send(uint32_t base, uint32_t value)
{
    while (!(SPI_SR(base) & 2u)) { // TXE
    }
    SPI_DR(base) = value;
    while (!(SPI_SR(base) & 1u)) { // RXNE: the frame is complete
    }
    (void)SPI_DR(base);
    while (SPI_SR(base) & 0x80u) { // BSY
    }
}

static void shift_register(uint32_t value)
{
    GPIO_BSRR(PORT_A) = 1u << (4 + 16); // latch low
    spi_send(SPI1_BASE, value);
    GPIO_BSRR(PORT_A) = 1u << 4; // the rising edge copies the bits to the outputs
}

static void max7219(uint32_t reg, uint32_t value)
{
    GPIO_BSRR(PORT_B) = 1u << (12 + 16);
    spi_send(SPI2_BASE, (reg << 8) | value);
    GPIO_BSRR(PORT_B) = 1u << 12; // LOAD rising: the word takes effect
}

static void print_temperature(void)
{
    struct line l;
    l.length = 0; // no initializer: without a C library there is no memset
    uint8_t raw[2];
    if (i2c1_read_register(LM75, 0, raw, 2) != 0) {
        line_text(&l, "lm75: no answer\r\n");
    } else {
        // Two's complement in units of 0.5 degree in the top nine bits.
        const int32_t halves = (int16_t)(uint16_t)((raw[0] << 8) | raw[1]) / 128;
        const uint32_t magnitude = (uint32_t)(halves < 0 ? -halves : halves);
        line_text(&l, halves < 0 ? "lm75 -" : "lm75 ");
        line_decimal(&l, magnitude / 2u, 1);
        line_text(&l, (magnitude & 1u) ? ".5\r\n" : ".0\r\n");
    }
    line_send(&l);
}

int main(void)
{
    RCC_AHB1ENR |= (1u << 0) | (1u << 1);          // GPIOAEN, GPIOBEN
    RCC_APB1ENR |= 1u << 14;                       // SPI2EN
    RCC_APB2ENR |= 1u << 12;                       // SPI1EN
    usart1_dma_open(139);                          // 16 MHz / 115200

    // PA4 and PB12 outputs (high); PA5, PA7, PB13, PB15 on function 5.
    GPIO_BSRR(PORT_A) = 1u << 4;
    GPIO_BSRR(PORT_B) = 1u << 12;
    GPIO_AFRL(PORT_A) = (GPIO_AFRL(PORT_A) & ~((15u << 20) | (15u << 28))) | (5u << 20) | (5u << 28);
    GPIO_MODER(PORT_A) = (GPIO_MODER(PORT_A) & ~((3u << 8) | (3u << 10) | (3u << 14)))
        | (1u << 8) | (2u << 10) | (2u << 14);
    GPIO_AFRH(PORT_B) = (GPIO_AFRH(PORT_B) & ~((15u << 20) | (15u << 28))) | (5u << 20) | (5u << 28);
    GPIO_MODER(PORT_B) = (GPIO_MODER(PORT_B) & ~((3u << 24) | (3u << 26) | (3u << 30)))
        | (1u << 24) | (2u << 26) | (2u << 30);
    // Masters with software NSS: SPI1 at PCLK2 / 16 (1 MHz), SPI2 at PCLK1 / 8 (2 MHz) with 16-bit frames.
    SPI_CR1(SPI1_BASE) = (1u << 2) | (3u << 3) | (1u << 8) | (1u << 9) | (1u << 6);
    SPI_CR1(SPI2_BASE) = (1u << 2) | (2u << 3) | (1u << 8) | (1u << 9) | (1u << 11) | (1u << 6);

    shift_register(0);
    static const uint8_t smiley[8] = {0x3C, 0x42, 0xA5, 0x81, 0xA5, 0x99, 0x42, 0x3C};
    max7219(0x0F, 0);    // display test off
    max7219(0x09, 0);    // no decoding
    max7219(0x0B, 7);    // scan all eight digits
    max7219(0x0A, 8);    // intensity
    for (uint32_t row = 0; row < 8u; ++row) {
        max7219(row + 1u, smiley[row]);
    }
    max7219(0x0C, 1);    // normal operation

    struct line l;
    l.length = 0; // no initializer: without a C library there is no memset
    line_text(&l, "f4_buses\r\n");
    line_send(&l);

    i2c1_open(16);
    static const uint8_t text[9] = {0x10, 'S', 'i', 'm', 'N', 'o', 'd', 'u', 's'};
    uint8_t back[9];
    back[8] = 0;
    if (i2c1_write(EEPROM, text, 9, 1) != 0) {
        line_text(&l, "eeprom: no answer\r\n");
    } else {
        // The EEPROM does not answer while it writes (about 5 ms): acknowledge polling.
        uint32_t tries = 0;
        while (i2c1_address(EEPROM, 0) != 0 && ++tries < 1000u) {
        }
        (void)I2C1_SR2;
        i2c1_stop();
        if (i2c1_read_register(EEPROM, 0x10, back, 8) != 0) {
            line_text(&l, "eeprom: no answer\r\n");
        } else {
            line_text(&l, "eeprom ");
            line_text(&l, (const char *)back);
            line_text(&l, "\r\n");
        }
    }
    line_send(&l);

    SYST_RVR = 16000u - 1u; // 1 ms
    SYST_CVR = 0;
    SYST_CSR = 7u;
    uint32_t count = 0, stepped = 0, measured = 0;
    for (;;) {
        __asm__ volatile("wfi");
        const uint32_t now = milliseconds;
        if (now - stepped >= 250u) {
            stepped += 250u;
            ++count;
            shift_register(count & 0xFFu);
            max7219(8, count & 0xFFu);
        }
        if (now - measured >= 1000u) {
            measured += 1000u;
            print_temperature();
        }
    }
}
