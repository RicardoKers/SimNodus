// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// Timers without libraries, for SimNodus and for a real Blue Pill (internal
// 8 MHz clock, so every timer counts at 8 MHz before its prescaler).
//   PA0 (TIM2_CH1): PWM 1 kHz, 25 % duty (PWM mode 1, CCR preload).
//   PA8 (TIM1_CH1) and PB13 (TIM1_CH1N): PWM 1 kHz, high for the last 10 %
//   of each period (PWM mode 2), PB13 the complement.
//   PC13 (on-board LED, lit when low): toggled by the TIM3 update interrupt
//   every 10 ms.
//   PB6 (TIM4_CH1): input capture of rising edges at 1 MHz; PB12 goes high
//   while the measured period is between 900 and 1100 us (wire PA0 to PB6).
#include <stdint.h>

#define REG(address) (*(volatile uint32_t *)(address))
#define RCC_APB2ENR REG(0x40021018u)
#define RCC_APB1ENR REG(0x4002101Cu)
#define GPIOA_CRL REG(0x40010800u)
#define GPIOA_CRH REG(0x40010804u)
#define GPIOB_CRL REG(0x40010C00u)
#define GPIOB_CRH REG(0x40010C04u)
#define GPIOB_BSRR REG(0x40010C10u)
#define GPIOC_CRH REG(0x40011004u)
#define GPIOC_ODR REG(0x4001100Cu)
#define NVIC_ISER0 REG(0xE000E100u)
#define SCB_VTOR REG(0xE000ED08u)

// Timer registers (RM0008 14.4 and 15.4).
#define TIM_CR1(t) REG((t) + 0x00u)
#define TIM_DIER(t) REG((t) + 0x0Cu)
#define TIM_SR(t) REG((t) + 0x10u)
#define TIM_EGR(t) REG((t) + 0x14u)
#define TIM_CCMR1(t) REG((t) + 0x18u)
#define TIM_CCER(t) REG((t) + 0x20u)
#define TIM_PSC(t) REG((t) + 0x28u)
#define TIM_ARR(t) REG((t) + 0x2Cu)
#define TIM_CCR1(t) REG((t) + 0x34u)
#define TIM_BDTR(t) REG((t) + 0x44u)
#define TIM1 0x40012C00u
#define TIM2 0x40000000u
#define TIM3 0x40000400u
#define TIM4 0x40000800u

extern uint32_t _data_load, _data_start, _data_end, _bss_start, _bss_end, _stack_top;

void Reset_Handler(void);
void Default_Handler(void);
void TIM3_IRQHandler(void);
void TIM4_IRQHandler(void);

#define D (uintptr_t)Default_Handler
// Core exceptions, then IRQ0..IRQ30 (TIM3 is IRQ29, TIM4 IRQ30).
__attribute__((section(".vectors"), used))
const uintptr_t vectors[47] = {
    (uintptr_t)&_stack_top, (uintptr_t)Reset_Handler, D, D, D, D, D, 0, 0, 0, 0, D, D, 0, D, D,
    D, D, D, D, D, D, D, D, D, D, // IRQ0..9
    D, D, D, D, D, D, D, D, D, D, // IRQ10..19
    D, D, D, D, D, D, D, D, D,    // IRQ20..28
    (uintptr_t)TIM3_IRQHandler, (uintptr_t)TIM4_IRQHandler,
};

volatile uint32_t ticks;
volatile uint32_t period_us;

void Default_Handler(void)
{
    for (;;) {
    }
}

void TIM3_IRQHandler(void)
{
    TIM_SR(TIM3) = ~1u; // clear UIF (write zero)
    ++ticks;
    GPIOC_ODR ^= 1u << 13u;
}

void TIM4_IRQHandler(void)
{
    static uint32_t last;
    static uint32_t seen;
    if (TIM_SR(TIM4) & (1u << 1u)) {
        const uint32_t now = TIM_CCR1(TIM4); // reading CCR1 clears CC1IF
        period_us = (now - last) & 0xFFFFu;
        last = now;
        if (seen++ > 0u) {
            GPIOB_BSRR = (period_us >= 900u && period_us <= 1100u) ? (1u << 12u) : (1u << (12u + 16u));
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
    // AFIOEN, IOPAEN, IOPBEN, IOPCEN, TIM1EN; TIM2EN, TIM3EN, TIM4EN.
    RCC_APB2ENR |= (1u << 0u) | (1u << 2u) | (1u << 3u) | (1u << 4u) | (1u << 11u);
    RCC_APB1ENR |= (1u << 0u) | (1u << 1u) | (1u << 2u);

    // Pins: PA0, PA8, PB13 alternate-function push-pull 2 MHz (0xA); PB6 floating
    // input (0x4); PB12 and PC13 push-pull outputs 2 MHz (0x2).
    GPIOA_CRL = (GPIOA_CRL & ~0xFu) | 0xAu;
    GPIOA_CRH = (GPIOA_CRH & ~0xFu) | 0xAu;
    GPIOB_CRL = (GPIOB_CRL & ~(0xFu << 24u)) | (0x4u << 24u);
    GPIOB_CRH = (GPIOB_CRH & ~(0xFFu << 16u)) | (0xA2u << 16u);
    GPIOC_CRH = (GPIOC_CRH & ~(0xFu << 20u)) | (0x2u << 20u);

    // TIM2: 1 MHz counter, 1000 counts per period, CH1 PWM mode 1 with preload.
    TIM_PSC(TIM2) = 7u;
    TIM_ARR(TIM2) = 999u;
    TIM_CCR1(TIM2) = 250u;
    TIM_CCMR1(TIM2) = (6u << 4u) | (1u << 3u); // OC1M = PWM 1, OC1PE
    TIM_CCER(TIM2) = 1u;                        // CC1E
    TIM_EGR(TIM2) = 1u;                         // UG: load PSC and CCR1
    TIM_CR1(TIM2) = (1u << 7u) | 1u;            // ARPE, CEN

    // TIM1: the same period, CH1 in PWM mode 2 (high from CCR1 = 900 on), CH1N complement.
    TIM_PSC(TIM1) = 7u;
    TIM_ARR(TIM1) = 999u;
    TIM_CCR1(TIM1) = 900u;
    TIM_CCMR1(TIM1) = (7u << 4u) | (1u << 3u); // OC1M = PWM 2, OC1PE
    TIM_CCER(TIM1) = (1u << 0u) | (1u << 2u);  // CC1E, CC1NE
    TIM_BDTR(TIM1) = 1u << 15u;                // MOE
    TIM_EGR(TIM1) = 1u;
    TIM_CR1(TIM1) = (1u << 7u) | 1u;

    // TIM3: 1 kHz counter, update every 10 counts (10 ms).
    TIM_PSC(TIM3) = 7999u;
    TIM_ARR(TIM3) = 9u;
    TIM_EGR(TIM3) = 1u;
    TIM_SR(TIM3) = 0u;
    TIM_DIER(TIM3) = 1u; // UIE
    TIM_CR1(TIM3) = 1u;

    // TIM4: 1 MHz free-running counter, CH1 captures rising edges of PB6 (TI1).
    TIM_PSC(TIM4) = 7u;
    TIM_ARR(TIM4) = 0xFFFFu;
    TIM_CCMR1(TIM4) = 1u; // CC1S = 01: input, IC1 on TI1
    TIM_CCER(TIM4) = 1u;  // CC1E, rising edge
    TIM_EGR(TIM4) = 1u;
    TIM_SR(TIM4) = 0u;
    TIM_DIER(TIM4) = 1u << 1u; // CC1IE
    TIM_CR1(TIM4) = 1u;

    NVIC_ISER0 = (1u << 29u) | (1u << 30u);
    for (;;) {
        __asm volatile("wfi");
    }
}
