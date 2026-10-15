// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// Blue Pill blink without libraries, for SimNodus and for a real board.
// The clock stays at its reset value (internal 8 MHz oscillator), so SysTick
// counts 8000 cycles per millisecond.
//   PC13 (on-board LED, lit when low): toggles every 500 ms.
//   PA0..PA3: a 4-bit counter that advances every 250 ms.
#include <stdint.h>

#define REG(address) (*(volatile uint32_t *)(address))
#define RCC_APB2ENR REG(0x40021018u)
#define GPIOA_CRL REG(0x40010800u)
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
    (uintptr_t)&_stack_top, (uintptr_t)Reset_Handler,
    (uintptr_t)Default_Handler, (uintptr_t)Default_Handler, // NMI, HardFault
    (uintptr_t)Default_Handler, (uintptr_t)Default_Handler, // MemManage, BusFault
    (uintptr_t)Default_Handler, 0, 0, 0, 0,                  // UsageFault, reserved
    (uintptr_t)Default_Handler, (uintptr_t)Default_Handler, // SVCall, DebugMon
    0, (uintptr_t)Default_Handler, (uintptr_t)SysTick_Handler, // reserved, PendSV, SysTick
};

static volatile uint32_t milliseconds;

void Default_Handler(void)
{
    for (;;) {
    }
}

void SysTick_Handler(void)
{
    const uint32_t now = ++milliseconds;
    if (now % 500u == 0u) {
        // Toggle PC13: BSRR sets with the low half, resets with the high half.
        const uint32_t on = (now / 500u) & 1u;
        GPIOC_BSRR = on ? (1u << (13u + 16u)) : (1u << 13u); // LED lit while PC13 is low
    }
    if (now % 250u == 0u) {
        const uint32_t count = (now / 250u) & 0xFu;
        GPIOA_BSRR = count | ((~count & 0xFu) << 16u);
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
    RCC_APB2ENR |= (1u << 2u) | (1u << 4u); // IOPAEN, IOPCEN
    GPIOA_CRL = (GPIOA_CRL & ~0xFFFFu) | 0x2222u; // PA0..PA3: 2 MHz push-pull outputs
    GPIOA_BSRR = 0xFu << 16u;
    GPIOC_CRH = (GPIOC_CRH & ~(0xFu << 20u)) | (0x2u << 20u); // PC13: 2 MHz push-pull output
    GPIOC_BSRR = 1u << 13u; // LED off
    SYST_RVR = 8000u - 1u; // 1 ms at 8 MHz
    SYST_CVR = 0u;
    SYST_CSR = 7u; // enable, interrupt, processor clock
    for (;;) {
        __asm volatile("wfi");
    }
}
