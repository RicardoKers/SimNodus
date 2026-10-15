// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// Benchmark mix for the SimNodus emulator (E-07, gate G0a.3): integer work that
// resembles firmware (CRC32, memory copies, sorting, division, recursion) with
// a 1 kHz SysTick interrupt, never sleeping. Runs at 72 MHz (HSE 8 MHz, PLL x9).
// PC13 toggles after every round; USART1 prints a checksum per round so that
// two emulators can be compared.
#include <stdint.h>

#define REG(address) (*(volatile uint32_t *)(address))
#define RCC_CR REG(0x40021000u)
#define RCC_CFGR REG(0x40021004u)
#define RCC_APB2ENR REG(0x40021018u)
#define FLASH_ACR REG(0x40022000u)
#define GPIOA_CRH REG(0x40010804u)
#define GPIOC_CRH REG(0x40011004u)
#define GPIOC_ODR REG(0x4001100Cu)
#define USART1_SR REG(0x40013800u)
#define USART1_DR REG(0x40013804u)
#define USART1_BRR REG(0x40013808u)
#define USART1_CR1 REG(0x4001380Cu)
#define SYST_CSR REG(0xE000E010u)
#define SYST_RVR REG(0xE000E014u)

extern uint32_t _data_load, _data_start, _data_end, _bss_start, _bss_end, _stack_top;

void Reset_Handler(void);
void Default_Handler(void);
void SysTick_Handler(void);

__attribute__((section(".vectors"), used))
const uintptr_t vectors[16] = {
    (uintptr_t)&_stack_top, (uintptr_t)Reset_Handler,
    (uintptr_t)Default_Handler, (uintptr_t)Default_Handler, (uintptr_t)Default_Handler,
    (uintptr_t)Default_Handler, (uintptr_t)Default_Handler, 0, 0, 0, 0,
    (uintptr_t)Default_Handler, (uintptr_t)Default_Handler, 0, (uintptr_t)Default_Handler,
    (uintptr_t)SysTick_Handler,
};

static volatile uint32_t ticks;
static uint32_t buffer_a[256];
static uint32_t buffer_b[256];

void Default_Handler(void)
{
    for (;;) {
    }
}

void SysTick_Handler(void)
{
    ++ticks;
}

static uint32_t crc32(const uint8_t* data, uint32_t length)
{
    uint32_t crc = 0xFFFFFFFFu;
    for (uint32_t i = 0; i < length; ++i) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; ++bit) {
            crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
        }
    }
    return ~crc;
}

static void copy(uint32_t* to, const uint32_t* from, uint32_t words)
{
    while (words--) {
        *to++ = *from++;
    }
}

static void sort(uint32_t* values, uint32_t count)
{
    for (uint32_t i = 1; i < count; ++i) {
        const uint32_t v = values[i];
        uint32_t j = i;
        while (j > 0 && values[j - 1] > v) {
            values[j] = values[j - 1];
            --j;
        }
        values[j] = v;
    }
}

static uint32_t fib(uint32_t n)
{
    return n < 2 ? n : fib(n - 1) + fib(n - 2);
}

static uint32_t divide_mix(uint32_t seed)
{
    uint32_t sum = 0;
    for (uint32_t i = 1; i < 200; ++i) {
        sum += seed / i + (int32_t)seed % (int32_t)(i * 3 + 1);
    }
    return sum;
}

static void put(char c)
{
    while (!(USART1_SR & (1u << 7))) {
    }
    USART1_DR = (uint32_t)c;
}

static void put_hex(uint32_t value)
{
    for (int shift = 28; shift >= 0; shift -= 4) {
        const uint32_t digit = (value >> shift) & 0xFu;
        put((char)(digit < 10 ? '0' + digit : 'A' + digit - 10));
    }
    put('\r');
    put('\n');
}

void Reset_Handler(void)
{
    for (uint32_t *src = &_data_load, *dst = &_data_start; dst < &_data_end;) {
        *dst++ = *src++;
    }
    for (uint32_t* dst = &_bss_start; dst < &_bss_end;) {
        *dst++ = 0;
    }
    // 72 MHz: two flash wait states, HSE on, PLL x9 from HSE, APB1 /2, switch.
    FLASH_ACR = 0x12;
    RCC_CR |= 1u << 16;
    while (!(RCC_CR & (1u << 17))) {
    }
    RCC_CFGR = (7u << 18) | (1u << 16) | (4u << 8);
    RCC_CR |= 1u << 24;
    while (!(RCC_CR & (1u << 25))) {
    }
    RCC_CFGR |= 2u;
    while ((RCC_CFGR & 0xCu) != 0x8u) {
    }
    RCC_APB2ENR |= (1u << 2) | (1u << 4) | (1u << 14); // GPIOA, GPIOC, USART1
    GPIOC_CRH = (GPIOC_CRH & ~(0xFu << 20)) | (0x2u << 20);
    GPIOA_CRH = (GPIOA_CRH & ~0xF0u) | 0xB0u;
    USART1_BRR = 625; // 115200 baud at 72 MHz
    USART1_CR1 = (1u << 13) | (1u << 3);
    SYST_RVR = 72000 - 1;
    SYST_CSR = 7;

    uint32_t seed = 12345;
    for (uint32_t round = 0;; ++round) {
        for (uint32_t i = 0; i < 256; ++i) {
            seed = seed * 1103515245u + 12345u;
            buffer_a[i] = seed;
        }
        copy(buffer_b, buffer_a, 256);
        sort(buffer_b, 256);
        uint32_t check = crc32((const uint8_t*)buffer_b, sizeof buffer_b);
        check ^= fib(18);
        check += divide_mix(seed);
        GPIOC_ODR ^= 1u << 13;
        put_hex(check ^ round);
    }
}
