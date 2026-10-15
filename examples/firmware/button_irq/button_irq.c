// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// Push button with an external interrupt, without libraries, for SimNodus and
// for a real Blue Pill (internal 8 MHz clock).
//   PA1: input with internal pull-up; a push button to ground pulls it low.
//   EXTI1 (both edges of PA1) wakes the CPU; a press (falling edge, PA1 low)
//   toggles the LED on PA0.
//   PC13 (on-board LED, lit when low) follows the button: lit while pressed.
#include <stdint.h>

#define REG(address) (*(volatile uint32_t *)(address))
#define RCC_APB2ENR REG(0x40021018u)
#define AFIO_EXTICR1 REG(0x40010008u)
#define GPIOA_CRL REG(0x40010800u)
#define GPIOA_IDR REG(0x40010808u)
#define GPIOA_ODR REG(0x4001080Cu)
#define GPIOA_BSRR REG(0x40010810u)
#define GPIOC_CRH REG(0x40011004u)
#define GPIOC_BSRR REG(0x40011010u)
#define EXTI_IMR REG(0x40010400u)
#define EXTI_RTSR REG(0x40010408u)
#define EXTI_FTSR REG(0x4001040Cu)
#define EXTI_PR REG(0x40010414u)
#define NVIC_ISER0 REG(0xE000E100u)
#define SCB_VTOR REG(0xE000ED08u)

extern uint32_t _data_load, _data_start, _data_end, _bss_start, _bss_end, _stack_top;

void Reset_Handler(void);
void Default_Handler(void);
void EXTI1_IRQHandler(void);

// Core exceptions, then IRQ0..IRQ7 (EXTI1 is IRQ7).
__attribute__((section(".vectors"), used))
const uintptr_t vectors[24] = {
    (uintptr_t)&_stack_top, (uintptr_t)Reset_Handler,
    (uintptr_t)Default_Handler, (uintptr_t)Default_Handler,
    (uintptr_t)Default_Handler, (uintptr_t)Default_Handler,
    (uintptr_t)Default_Handler, 0, 0, 0, 0,
    (uintptr_t)Default_Handler, (uintptr_t)Default_Handler,
    0, (uintptr_t)Default_Handler, (uintptr_t)Default_Handler,
    (uintptr_t)Default_Handler, (uintptr_t)Default_Handler, // IRQ0 WWDG, IRQ1 PVD
    (uintptr_t)Default_Handler, (uintptr_t)Default_Handler, // IRQ2 TAMPER, IRQ3 RTC
    (uintptr_t)Default_Handler, (uintptr_t)Default_Handler, // IRQ4 FLASH, IRQ5 RCC
    (uintptr_t)Default_Handler, (uintptr_t)EXTI1_IRQHandler, // IRQ6 EXTI0, IRQ7 EXTI1
};

volatile uint32_t presses;

void Default_Handler(void)
{
    for (;;) {
    }
}

void EXTI1_IRQHandler(void)
{
    if (EXTI_PR & (1u << 1u)) {
        EXTI_PR = 1u << 1u; // write one to clear
        if ((GPIOA_IDR & (1u << 1u)) == 0u) { // pressed
            ++presses;
            GPIOA_ODR ^= 1u; // toggle the LED on PA0
        }
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
    RCC_APB2ENR |= (1u << 0u) | (1u << 2u) | (1u << 4u); // AFIOEN, IOPAEN, IOPCEN
    // PA0: 2 MHz push-pull output; PA1: input with pull-up/down (CNF 10), ODR bit 1 selects up.
    GPIOA_CRL = (GPIOA_CRL & ~0xFFu) | 0x82u;
    GPIOA_BSRR = (1u << 1u) | (1u << 16u); // pull-up on PA1, LED on PA0 off
    GPIOC_CRH = (GPIOC_CRH & ~(0xFu << 20u)) | (0x2u << 20u); // PC13: push-pull output
    GPIOC_BSRR = 1u << 13u;                                    // on-board LED off
    AFIO_EXTICR1 &= ~(0xFu << 4u); // EXTI1 from port A
    EXTI_FTSR |= 1u << 1u;         // press
    EXTI_RTSR |= 1u << 1u;         // release (wakes the loop that shows the button)
    EXTI_PR = 1u << 1u;
    EXTI_IMR |= 1u << 1u;
    NVIC_ISER0 = 1u << 7u;
    for (;;) {
        // Between interrupts: the on-board LED shows the button (lit while PA1 is low).
        GPIOC_BSRR = (GPIOA_IDR & (1u << 1u)) ? (1u << 13u) : (1u << (13u + 16u));
        __asm volatile("wfi");
    }
}
