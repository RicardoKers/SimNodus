// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// STM32F407 push button on an external interrupt (experiment E-12).
//   PA0: the button to ground, with the pin's pull-up.
//   PC13: follows the button (high while it is pressed).
//   PB0..PB3: a 4-bit count of the presses.
// EXTI0 fires on both edges; SYSCFG leaves line 0 on port A after a reset.
#define F4_EXTI0 EXTI0_IRQHandler
#include "../f4_common/f4.h"

static volatile uint32_t presses;

void EXTI0_IRQHandler(void)
{
    EXTI_PR = 1u; // written with 1 to clear
    const uint32_t pressed = !(GPIO_IDR(PORT_A) & 1u);
    GPIO_BSRR(PORT_C) = pressed ? (1u << 13) : (1u << (13 + 16));
    if (pressed) {
        const uint32_t count = ++presses & 0xFu;
        GPIO_BSRR(PORT_B) = count | ((~count & 0xFu) << 16);
    }
}

int main(void)
{
    RCC_AHB1ENR |= (1u << 0) | (1u << 1) | (1u << 2); // GPIOAEN, GPIOBEN, GPIOCEN
    RCC_APB2ENR |= 1u << 14;                           // SYSCFGEN
    GPIO_PUPDR(PORT_A) = (GPIO_PUPDR(PORT_A) & ~3u) | 1u; // PA0: pull-up (an input after reset)
    GPIO_MODER(PORT_B) = (GPIO_MODER(PORT_B) & ~0xFFu) | 0x55u;
    GPIO_MODER(PORT_C) = (GPIO_MODER(PORT_C) & ~(3u << 26)) | (1u << 26);
    EXTI_RTSR |= 1u;
    EXTI_FTSR |= 1u;
    EXTI_IMR |= 1u;
    NVIC_ISER0 = 1u << 6; // EXTI0
    for (;;) {
        __asm__ volatile("wfi");
    }
}
