// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// STM32F407 blink without libraries, for SimNodus and for a real chip
// (experiment E-12). The clock stays at its reset value (the internal 16 MHz
// oscillator), so SysTick counts 16000 cycles per millisecond.
//   PC13: toggles every 500 ms.
//   PA0..PA3: a 4-bit counter that advances every 250 ms.
//   PB0: toggled by a busy loop of 1 000 000 turns, whose period is the
//        cycle count of the compiled code.
#define F4_SYSTICK SysTick_Handler
#include "../f4_common/f4.h"

static volatile uint32_t milliseconds;

void SysTick_Handler(void)
{
    const uint32_t now = ++milliseconds;
    if (now % 500u == 0u) {
        const uint32_t on = (now / 500u) & 1u;
        GPIO_BSRR(PORT_C) = on ? (1u << 13) : (1u << (13 + 16));
    }
    if (now % 250u == 0u) {
        const uint32_t count = (now / 250u) & 0xFu;
        GPIO_BSRR(PORT_A) = count | ((~count & 0xFu) << 16);
    }
}

int main(void)
{
    RCC_AHB1ENR |= (1u << 0) | (1u << 1) | (1u << 2); // GPIOAEN, GPIOBEN, GPIOCEN
    GPIO_MODER(PORT_A) = (GPIO_MODER(PORT_A) & ~0xFFu) | 0x55u; // PA0..PA3 outputs
    GPIO_MODER(PORT_B) = (GPIO_MODER(PORT_B) & ~3u) | 1u;       // PB0 output
    GPIO_MODER(PORT_C) = (GPIO_MODER(PORT_C) & ~(3u << 26)) | (1u << 26); // PC13 output
    SYST_RVR = 16000u - 1u;
    SYST_CVR = 0;
    SYST_CSR = 7u; // processor clock, interrupt, enable
    for (;;) {
        for (volatile uint32_t turn = 0; turn < 1000000u; ++turn) {
        }
        GPIO_ODR(PORT_B) ^= 1u;
    }
}
