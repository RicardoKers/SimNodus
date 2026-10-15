// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// Serial port example without libraries, for SimNodus and for a real Blue Pill
// (internal 8 MHz clock, USART1 at 115200 baud on PA9 TX / PA10 RX).
//   Every second it prints "tick N".
//   Received characters are echoed from the RX interrupt; '1' turns the LED
//   on PA0 on, '0' turns it off.
#include <stdint.h>

#define REG(address) (*(volatile uint32_t *)(address))
#define RCC_APB2ENR REG(0x40021018u)
#define GPIOA_CRL REG(0x40010800u)
#define GPIOA_CRH REG(0x40010804u)
#define GPIOA_BSRR REG(0x40010810u)
#define USART1_SR REG(0x40013800u)
#define USART1_DR REG(0x40013804u)
#define USART1_BRR REG(0x40013808u)
#define USART1_CR1 REG(0x4001380Cu)
#define NVIC_ISER1 REG(0xE000E104u)
#define SYST_CSR REG(0xE000E010u)
#define SYST_RVR REG(0xE000E014u)
#define SYST_CVR REG(0xE000E018u)
#define SCB_VTOR REG(0xE000ED08u)

#define SR_RXNE (1u << 5u)
#define SR_TXE (1u << 7u)

extern uint32_t _data_load, _data_start, _data_end, _bss_start, _bss_end, _stack_top;

void Reset_Handler(void);
void Default_Handler(void);
void SysTick_Handler(void);
void USART1_IRQHandler(void);

// Core exceptions and IRQ0..IRQ37 (USART1 is IRQ37).
__attribute__((section(".vectors"), used))
const uintptr_t vectors[16 + 38] = {
    [0] = (uintptr_t)&_stack_top,
    [1] = (uintptr_t)Reset_Handler,
    [2 ... 6] = (uintptr_t)Default_Handler,
    [11] = (uintptr_t)Default_Handler,
    [12] = (uintptr_t)Default_Handler,
    [14] = (uintptr_t)Default_Handler,
    [15] = (uintptr_t)SysTick_Handler,
    [16 ... 52] = (uintptr_t)Default_Handler,
    [16 + 37] = (uintptr_t)USART1_IRQHandler,
};

static volatile uint32_t milliseconds;
static volatile uint32_t tick_due;

void Default_Handler(void)
{
    for (;;) {
    }
}

static void put_char(char c)
{
    while (!(USART1_SR & SR_TXE)) {
    }
    USART1_DR = (uint8_t)c;
}

static void put_string(const char *text)
{
    while (*text) {
        put_char(*text++);
    }
}

static void put_number(uint32_t value)
{
    char digits[10];
    int count = 0;
    do {
        digits[count++] = (char)('0' + value % 10u);
        value /= 10u;
    } while (value && count < 10);
    while (count) {
        put_char(digits[--count]);
    }
}

void SysTick_Handler(void)
{
    if (++milliseconds % 1000u == 0u) {
        tick_due = 1;
    }
}

void USART1_IRQHandler(void)
{
    if (USART1_SR & SR_RXNE) {
        const char c = (char)USART1_DR; // reading DR clears RXNE
        if (c == '1') {
            GPIOA_BSRR = 1u; // LED on
        } else if (c == '0') {
            GPIOA_BSRR = 1u << 16u; // LED off
        }
        put_char(c); // echo
    }
}

void Reset_Handler(void)
{
    uint32_t *source = &_data_load;
    for (uint32_t *target = &_data_start; target < &_data_end;) {
        *target++ = *source++;
    }
    for (uint32_t *target = &_bss_start; target < &_bss_end;) {
        *target++ = 0;
    }
    SCB_VTOR = 0x08000000u;
    RCC_APB2ENR |= (1u << 0u) | (1u << 2u) | (1u << 14u); // AFIOEN, IOPAEN, USART1EN
    GPIOA_CRL = (GPIOA_CRL & ~0xFu) | 0x2u;                // PA0: LED output
    GPIOA_BSRR = 1u << 16u;
    // PA9: alternate function push-pull (TX); PA10: floating input (RX).
    GPIOA_CRH = (GPIOA_CRH & ~0xFF0u) | (0xBu << 4u) | (0x4u << 8u);
    USART1_BRR = 69u;                                         // 8 MHz / 115200
    USART1_CR1 = (1u << 13u) | (1u << 5u) | (1u << 3u) | (1u << 2u); // UE, RXNEIE, TE, RE
    NVIC_ISER1 = 1u << (37u - 32u);
    SYST_RVR = 8000u - 1u;
    SYST_CVR = 0u;
    SYST_CSR = 7u;
    put_string("SimNodus Blue Pill ready. Send 1 or 0 to switch the LED.\r\n");
    uint32_t ticks = 0;
    for (;;) {
        if (tick_due) {
            tick_due = 0;
            put_string("tick ");
            put_number(++ticks);
            put_string("\r\n");
        }
        __asm volatile("wfi");
    }
}
