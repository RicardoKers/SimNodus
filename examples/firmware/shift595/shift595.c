// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// Blue Pill and a 74HC595 shift register, without libraries (SN-203): the
// firmware shifts a byte out bit by bit on the pins SPI1 uses, so the same
// circuit can later be driven by the SPI peripheral.
//   PA7 -> DS (data), PA5 -> SHCP (shift clock), PA4 -> STCP (latch).
//   A light runs across Q0..Q7 and back, one step every 100 ms; after each
//   round the eight LEDs count in binary for a while.
//   PC13 (on-board LED, lit when low): toggles with every byte.
// Internal 8 MHz clock; SysTick counts milliseconds.
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

#define DS (1u << 7u)
#define SHCP (1u << 5u)
#define STCP (1u << 4u)

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
    ++milliseconds;
}

static void wait_ms(uint32_t ms)
{
    const uint32_t start = milliseconds;
    while (milliseconds - start < ms) {
        __asm volatile("wfi");
    }
}

// Most significant bit first: after eight clocks bit 7 is in Q7, bit 0 in Q0.
static void shift_out(uint8_t value)
{
    for (int bit = 7; bit >= 0; --bit) {
        GPIOA_BSRR = ((value >> bit) & 1u) ? DS : (DS << 16u);
        GPIOA_BSRR = SHCP;
        GPIOA_BSRR = SHCP << 16u;
    }
    GPIOA_BSRR = STCP; // the outputs change now
    GPIOA_BSRR = STCP << 16u;
    static uint32_t toggle;
    toggle ^= 1u;
    GPIOC_BSRR = toggle ? (1u << (13u + 16u)) : (1u << 13u);
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
    // PA4, PA5, PA7: 10 MHz push-pull outputs.
    GPIOA_CRL = (GPIOA_CRL & ~((0xFu << 16u) | (0xFu << 20u) | (0xFu << 28u)))
        | (0x1u << 16u) | (0x1u << 20u) | (0x1u << 28u);
    GPIOA_BSRR = (DS | SHCP | STCP) << 16u;
    GPIOC_CRH = (GPIOC_CRH & ~(0xFu << 20u)) | (0x2u << 20u); // PC13: 2 MHz push-pull output
    GPIOC_BSRR = 1u << 13u; // LED off
    SYST_RVR = 8000u - 1u; // 1 ms at 8 MHz
    SYST_CVR = 0u;
    SYST_CSR = 7u; // enable, interrupt, processor clock
    for (;;) {
        for (int i = 0; i < 8; ++i) {
            shift_out((uint8_t)(1u << i));
            wait_ms(100);
        }
        for (int i = 6; i > 0; --i) {
            shift_out((uint8_t)(1u << i));
            wait_ms(100);
        }
        for (uint32_t count = 0; count < 16u; ++count) {
            shift_out((uint8_t)count);
            wait_ms(100);
        }
    }
}
