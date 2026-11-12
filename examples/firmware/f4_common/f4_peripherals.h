// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// What f4_buses, f4_rtc and f4_station share (experiment E-12, SN-298):
// I2C1 as a master on PB6 (SCL) and PB7 (SDA), lines of text sent on
// USART1 through DMA2 stream 7, and the RTC started on the LSE crystal (or
// the LSI without one). Register addresses from RM0090; include it instead
// of f4.h.
#ifndef SIMNODUS_F4_PERIPHERALS_H
#define SIMNODUS_F4_PERIPHERALS_H

#include "f4.h"

// ---------------------------------------------------------------------------
// I2C1, 100 kHz, polled.

#define I2C1_CR1 REG(0x40005400u)
#define I2C1_CR2 REG(0x40005404u)
#define I2C1_DR REG(0x40005410u)
#define I2C1_SR1 REG(0x40005414u)
#define I2C1_SR2 REG(0x40005418u)
#define I2C1_CCR REG(0x4000541Cu)
#define I2C1_TRISE REG(0x40005420u)

enum {
    I2C_START = 1u << 8, I2C_STOP = 1u << 9, I2C_ACK = 1u << 10,
    I2C_SB = 1u << 0, I2C_ADDR = 1u << 1, I2C_BTF = 1u << 2, I2C_RXNE = 1u << 6, I2C_TXE = 1u << 7, I2C_AF = 1u << 10,
};

// PB6 and PB7 open drain on function 4 (the pull-up resistors are on the board).
static inline void i2c1_open(uint32_t pclk1_mhz)
{
    RCC_AHB1ENR |= 1u << 1;  // GPIOBEN
    RCC_APB1ENR |= 1u << 21; // I2C1EN
    GPIO_OTYPER(PORT_B) |= 3u << 6;
    GPIO_AFRL(PORT_B) = (GPIO_AFRL(PORT_B) & ~(0xFFu << 24)) | (0x44u << 24);
    GPIO_MODER(PORT_B) = (GPIO_MODER(PORT_B) & ~(0xFu << 12)) | (0xAu << 12);
    I2C1_CR2 = pclk1_mhz;              // FREQ
    I2C1_CCR = pclk1_mhz * 5u;         // standard mode: 5 us high, 5 us low
    I2C1_TRISE = pclk1_mhz + 1u;       // 1000 ns
    I2C1_CR1 = 1u;                     // PE
}

static inline void i2c1_stop(void)
{
    I2C1_CR1 |= I2C_STOP;
    while (I2C1_CR1 & I2C_STOP) { // cleared when the STOP condition is on the bus
    }
}

// START (or a repeated START) and the address. 0 if a chip acknowledged it
// (ADDR is then set; reading SR2 clears it), -1 if none did (the bus is stopped).
static inline int i2c1_address(uint32_t address, uint32_t read)
{
    I2C1_CR1 |= I2C_START;
    while (!(I2C1_SR1 & I2C_SB)) {
    }
    I2C1_DR = (address << 1) | read;
    for (;;) {
        const uint32_t sr1 = I2C1_SR1;
        if (sr1 & I2C_ADDR) {
            return 0;
        }
        if (sr1 & I2C_AF) {
            I2C1_SR1 = ~(uint32_t)I2C_AF & 0xFFFFu;
            i2c1_stop();
            return -1;
        }
    }
}

// Writes `count` bytes to the chip at `address`. `stop` 0 keeps the bus for
// a repeated START (a register pointer before a read). 0 if every byte was acknowledged.
static inline int i2c1_write(uint32_t address, const uint8_t *data, int count, int stop)
{
    if (i2c1_address(address, 0) != 0) {
        return -1;
    }
    (void)I2C1_SR2;
    for (int i = 0; i <= count; ++i) {
        const uint32_t wanted = i < count ? I2C_TXE : I2C_BTF;
        uint32_t sr1;
        do {
            sr1 = I2C1_SR1;
        } while (!(sr1 & (wanted | I2C_AF)));
        if (sr1 & I2C_AF) {
            I2C1_SR1 = ~(uint32_t)I2C_AF & 0xFFFFu;
            i2c1_stop();
            return -1;
        }
        if (i < count) {
            I2C1_DR = data[i];
        }
    }
    if (stop) {
        i2c1_stop();
    }
    return 0;
}

// Reads `count` bytes; the last one is not acknowledged and a STOP follows it.
static inline int i2c1_read(uint32_t address, uint8_t *data, int count)
{
    if (count == 1) {
        I2C1_CR1 &= ~(uint32_t)I2C_ACK; // before ADDR clears
    } else {
        I2C1_CR1 |= I2C_ACK;
    }
    if (i2c1_address(address, 1) != 0) {
        return -1;
    }
    (void)I2C1_SR2;
    for (int i = 0; i < count; ++i) {
        if (i == count - 1) {
            I2C1_CR1 = (I2C1_CR1 & ~(uint32_t)I2C_ACK) | I2C_STOP; // while the last byte arrives
        }
        while (!(I2C1_SR1 & I2C_RXNE)) {
        }
        data[i] = (uint8_t)I2C1_DR;
    }
    while (I2C1_CR1 & I2C_STOP) {
    }
    return 0;
}

// Register `reg` of the chip at `address`, `count` bytes (a write of the pointer, then a read).
static inline int i2c1_read_register(uint32_t address, uint8_t reg, uint8_t *data, int count)
{
    if (i2c1_write(address, &reg, 1, 0) != 0) {
        return -1;
    }
    return i2c1_read(address, data, count);
}

// ---------------------------------------------------------------------------
// Lines of text on USART1 through DMA2 stream 7, channel 4.

#define DMA2_HIFCR REG(0x4002640Cu)
#define DMA2_S7CR REG(0x400264B8u)
#define DMA2_S7NDTR REG(0x400264BCu)
#define DMA2_S7PAR REG(0x400264C0u)
#define DMA2_S7M0AR REG(0x400264C4u)

struct line {
    char text[96];
    uint32_t length;
};

static inline void usart1_dma_open(uint32_t brr)
{
    usart1_start(brr);
    RCC_AHB1ENR |= 1u << 22; // DMA2EN
    USART1_CR3 |= 1u << 7;   // DMAT
}

static inline void line_text(struct line *l, const char *text)
{
    while (*text && l->length < sizeof l->text) {
        l->text[l->length++] = *text++;
    }
}

static inline void line_char(struct line *l, char c)
{
    if (l->length < sizeof l->text) {
        l->text[l->length++] = c;
    }
}

// `value` in decimal with at least `width` digits.
static inline void line_decimal(struct line *l, uint32_t value, int width)
{
    char digits[10];
    int count = 0;
    do {
        digits[count++] = (char)('0' + value % 10u);
        value /= 10u;
    } while (value != 0u || count < width);
    while (count > 0) {
        line_char(l, digits[--count]);
    }
}

// Two BCD digits (the RTC's registers).
static inline void line_bcd(struct line *l, uint32_t value)
{
    line_char(l, (char)('0' + ((value >> 4) & 15u)));
    line_char(l, (char)('0' + (value & 15u)));
}

// Sends the line once the previous one has left the buffer; the firmware goes on meanwhile.
static inline void line_send(struct line *l)
{
    static char dma_buffer[96]; // what the DMA is sending
    while (DMA2_S7CR & 1u) { // EN: the previous line is still going out
    }
    for (uint32_t i = 0; i < l->length; ++i) {
        dma_buffer[i] = l->text[i];
    }
    DMA2_HIFCR = 0x3Du << 22; // stream 7's flags
    DMA2_S7PAR = 0x40011004u; // USART1_DR
    DMA2_S7M0AR = (uint32_t)(uintptr_t)dma_buffer;
    DMA2_S7NDTR = l->length;
    DMA2_S7CR = (4u << 25) | (1u << 10) | (1u << 6) | 1u; // channel 4, MINC, memory to peripheral, EN
    l->length = 0;
}

// ---------------------------------------------------------------------------
// The RTC.

#define PWR_CR REG(0x40007000u)
#define RCC_BDCR REG(0x40023870u)
#define RTC_TR REG(0x40002800u)
#define RTC_DR REG(0x40002804u)
#define RTC_CR REG(0x40002808u)
#define RTC_ISR REG(0x4000280Cu)
#define RTC_PRER REG(0x40002810u)
#define RTC_WUTR REG(0x40002814u)
#define RTC_ALRMAR REG(0x4000281Cu)
#define RTC_WPR REG(0x40002824u)
#define RTC_BKP(n) REG(0x40002850u + 4u * (n))

#define RTC_MAGIC 0x32F2u // in backup register 0 once the calendar is set

// Clears ISR flags (rc_w0) without touching INIT.
static inline void rtc_clear(uint32_t flags)
{
    RTC_ISR = ~(flags | (1u << 7)) & 0x1FFFFu;
}

// Starts the RTC on the LSE crystal, or on the LSI if the LSE is not ready
// within about a second. On the first start (backup register 0 is not
// RTC_MAGIC) the calendar is set to `time` and `date` (BCD, as RTC_TR and
// RTC_DR); later it keeps counting. Returns 1 if the calendar was set.
// Backup register 2 records the clock: 1 the LSE, 2 the LSI.
static inline int rtc_open(uint32_t time, uint32_t date)
{
    RCC_APB1ENR |= 1u << 28; // PWREN
    PWR_CR |= 1u << 8;       // DBP: the backup domain can be written
    if (RTC_BKP(0) == RTC_MAGIC) {
        if (RTC_BKP(2) == 2u) {
            RCC_CSR |= 1u; // the LSI stops at every reset; the RTC needs it again
            while (!(RCC_CSR & 2u)) {
            }
        }
        return 0;
    }
    RCC_BDCR = 1u << 16; // BDRST: a clean backup domain
    RCC_BDCR = 0;
    RCC_BDCR = 1u; // LSEON
    uint32_t wait = 0;
    while (!(RCC_BDCR & 2u) && ++wait < 4000000u) { // LSERDY
    }
    uint32_t prescalers;
    if (RCC_BDCR & 2u) {
        RCC_BDCR |= (1u << 8) | (1u << 15); // RTCSEL = LSE, RTCEN
        prescalers = (127u << 16) | 255u;   // 32768 / 128 / 256 = 1 Hz
        RTC_BKP(2) = 1u;
    } else {
        RCC_BDCR = 0;
        RCC_CSR |= 1u; // LSION
        while (!(RCC_CSR & 2u)) {
        }
        RCC_BDCR = (2u << 8) | (1u << 15); // RTCSEL = LSI, RTCEN
        prescalers = (127u << 16) | 249u;  // 32000 / 128 / 250 = 1 Hz
        RTC_BKP(2) = 2u;
    }
    RTC_WPR = 0xCAu;
    RTC_WPR = 0x53u;
    RTC_ISR = 1u << 7; // INIT
    while (!(RTC_ISR & (1u << 6))) { // INITF
    }
    RTC_PRER = prescalers & 0x7FFFu; // PREDIV_S, then PREDIV_A: two writes (RM0090 26.3.5)
    RTC_PRER = prescalers;
    RTC_TR = time;
    RTC_DR = date;
    RTC_ISR = 0; // leaves INIT: the calendar counts
    RTC_WPR = 0xFFu;
    RTC_BKP(0) = RTC_MAGIC;
    return 1;
}

// The time and date as "hh:mm:ss yyyy-mm-dd".
static inline void line_time(struct line *l)
{
    const uint32_t tr = RTC_TR; // reading TR holds DR until it is read
    const uint32_t dr = RTC_DR;
    line_bcd(l, (tr >> 16) & 0x3Fu);
    line_char(l, ':');
    line_bcd(l, (tr >> 8) & 0x7Fu);
    line_char(l, ':');
    line_bcd(l, tr & 0x7Fu);
    line_text(l, " 20");
    line_bcd(l, (dr >> 16) & 0xFFu);
    line_char(l, '-');
    line_bcd(l, (dr >> 8) & 0x1Fu);
    line_char(l, '-');
    line_bcd(l, dr & 0x3Fu);
}

#endif
