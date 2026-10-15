// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// Lesson 1 - GPIO output (examples/lessons/lesson1_gpio_output.md).
// Blue Pill without libraries, internal 8 MHz clock; runs on a real board too.
//   PA0: LED with a series resistor, on for 500 ms, off for 500 ms.
//   PC13: on-board LED (lit when low), lit while the PA0 LED is on.
//   PA1: 1 kHz square wave (toggles every 0.5 ms) into an RC low-pass filter.
#include <stdint.h>

#define REG(address) (*(volatile uint32_t *)(address))
#define RCC_APB2ENR REG(0x40021018u)
#define GPIOA_CRL REG(0x40010800u)
#define GPIOA_ODR REG(0x4001080Cu)
#define GPIOA_BSRR REG(0x40010810u)
#define GPIOC_CRH REG(0x40011004u)
#define GPIOC_BSRR REG(0x40011010u)
#define SYST_CSR REG(0xE000E010u)
#define SYST_RVR REG(0xE000E014u)
#define SYST_CVR REG(0xE000E018u)
#define SCB_VTOR REG(0xE000ED08u)

extern uint32_t _data_load, _data_start, _data_end, _bss_start, _bss_end, _stack_top;

void Reset_Handler(void);
void Default_Handler(void);
void SysTick_Handler(void);

__attribute__((section(".vectors"), used))
const uintptr_t vectors[16] = {
    [0] = (uintptr_t)&_stack_top,
    [1] = (uintptr_t)Reset_Handler,
    [2 ... 6] = (uintptr_t)Default_Handler,
    [11] = (uintptr_t)Default_Handler,
    [12] = (uintptr_t)Default_Handler,
    [14] = (uintptr_t)Default_Handler,
    [15] = (uintptr_t)SysTick_Handler,
};

static volatile uint32_t ticks; // half milliseconds

void Default_Handler(void)
{
    for (;;) {
    }
}

void SysTick_Handler(void)
{
    ++ticks;
    GPIOA_ODR ^= 1u << 1u; // PA1: one edge every 0.5 ms -> 1 kHz
    if (ticks % 1000u == 0u) {
        const uint32_t led_on = (ticks / 1000u) & 1u;
        GPIOA_BSRR = led_on ? 1u : (1u << 16u);                    // PA0 LED
        GPIOC_BSRR = led_on ? (1u << (13u + 16u)) : (1u << 13u); // PC13 lit when low
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
    RCC_APB2ENR |= (1u << 2u) | (1u << 4u);         // IOPAEN, IOPCEN
    GPIOA_CRL = (GPIOA_CRL & ~0xFFu) | 0x22u;       // PA0, PA1: 2 MHz push-pull outputs
    GPIOA_BSRR = (1u << 16u) | (1u << 17u);         // both low
    GPIOC_CRH = (GPIOC_CRH & ~(0xFu << 20u)) | (0x2u << 20u);
    GPIOC_BSRR = 1u << 13u;                         // on-board LED off
    SYST_RVR = 4000u - 1u;                          // 0.5 ms at 8 MHz
    SYST_CVR = 0u;
    SYST_CSR = 7u;
    for (;;) {
        __asm volatile("wfi");
    }
}
