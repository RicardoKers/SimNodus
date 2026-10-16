// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// ADC to PWM without libraries, for SimNodus and for a real Blue Pill
// (internal 8 MHz clock).
//   PA1 (ADC1 IN1): continuous conversions, 239.5-cycle sample time (63 us per
//   conversion at the 4 MHz ADC clock); the end-of-conversion interrupt keeps
//   the latest code.
//   PA0 (TIM2_CH1): PWM 1 kHz whose duty follows the code (0..4095 -> 0..100 %).
//   PC13 (on-board LED, lit when low): lit while the code is above 2048.
//   USART1 (PA9 TX, 115200 baud): "ADC <code> = <millivolts> mV" every 500 ms.
#include <stdint.h>

#define REG(address) (*(volatile uint32_t *)(address))
#define RCC_APB2ENR REG(0x40021018u)
#define RCC_APB1ENR REG(0x4002101Cu)
#define GPIOA_CRL REG(0x40010800u)
#define GPIOA_CRH REG(0x40010804u)
#define GPIOC_CRH REG(0x40011004u)
#define GPIOC_BSRR REG(0x40011010u)
#define USART1_SR REG(0x40013800u)
#define USART1_DR REG(0x40013804u)
#define USART1_BRR REG(0x40013808u)
#define USART1_CR1 REG(0x4001380Cu)
#define ADC1_SR REG(0x40012400u)
#define ADC1_CR1 REG(0x40012404u)
#define ADC1_CR2 REG(0x40012408u)
#define ADC1_SMPR2 REG(0x40012410u)
#define ADC1_SQR3 REG(0x40012434u)
#define ADC1_DR REG(0x4001244Cu)
#define TIM2_CR1 REG(0x40000000u)
#define TIM2_EGR REG(0x40000014u)
#define TIM2_CCMR1 REG(0x40000018u)
#define TIM2_CCER REG(0x40000020u)
#define TIM2_PSC REG(0x40000028u)
#define TIM2_ARR REG(0x4000002Cu)
#define TIM2_CCR1 REG(0x40000034u)
#define SYST_CSR REG(0xE000E010u)
#define SYST_RVR REG(0xE000E014u)
#define SYST_CVR REG(0xE000E018u)
#define NVIC_ISER0 REG(0xE000E100u)
#define SCB_VTOR REG(0xE000ED08u)

#define SR_TXE (1u << 7u)

extern uint32_t _data_load, _data_start, _data_end, _bss_start, _bss_end, _stack_top;

void Reset_Handler(void);
void Default_Handler(void);
void SysTick_Handler(void);
void ADC1_2_IRQHandler(void);

// Core exceptions and IRQ0..IRQ18 (ADC1_2 is IRQ18).
__attribute__((section(".vectors"), used))
const uintptr_t vectors[16 + 19] = {
    [0] = (uintptr_t)&_stack_top,
    [1] = (uintptr_t)Reset_Handler,
    [2 ... 6] = (uintptr_t)Default_Handler,
    [11] = (uintptr_t)Default_Handler,
    [12] = (uintptr_t)Default_Handler,
    [14] = (uintptr_t)Default_Handler,
    [15] = (uintptr_t)SysTick_Handler,
    [16 ... 33] = (uintptr_t)Default_Handler,
    [16 + 18] = (uintptr_t)ADC1_2_IRQHandler,
};

static volatile uint32_t milliseconds;
static volatile uint32_t report_due;
volatile uint32_t adc_code;

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
    if (++milliseconds % 500u == 0u) {
        report_due = 1;
    }
}

void ADC1_2_IRQHandler(void)
{
    const uint32_t code = ADC1_DR & 0xFFFu; // reading DR clears EOC
    adc_code = code;
    TIM2_CCR1 = code * 1000u / 4096u;       // preloaded: takes effect at the next period
    GPIOC_BSRR = code > 2048u ? (1u << (13u + 16u)) : (1u << 13u);
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
    // AFIOEN, IOPAEN, IOPCEN, ADC1EN, USART1EN; TIM2EN.
    RCC_APB2ENR |= (1u << 0u) | (1u << 2u) | (1u << 4u) | (1u << 9u) | (1u << 14u);
    RCC_APB1ENR |= 1u << 0u;

    // PA0: alternate-function push-pull 2 MHz (0xA); PA1: analog (0x0);
    // PA9: alternate-function push-pull 50 MHz (0xB); PA10: floating input (0x4).
    GPIOA_CRL = (GPIOA_CRL & ~0xFFu) | 0x0Au;
    GPIOA_CRH = (GPIOA_CRH & ~0xFF0u) | (0xBu << 4u) | (0x4u << 8u);
    GPIOC_CRH = (GPIOC_CRH & ~(0xFu << 20u)) | (0x2u << 20u);
    GPIOC_BSRR = 1u << 13u;

    USART1_BRR = 69u;                       // 8 MHz / 115200
    USART1_CR1 = (1u << 13u) | (1u << 3u);  // UE, TE

    // TIM2: 1 MHz counter, 1000 counts per period, CH1 PWM mode 1 with preload.
    TIM2_PSC = 7u;
    TIM2_ARR = 999u;
    TIM2_CCMR1 = (6u << 4u) | (1u << 3u);
    TIM2_CCER = 1u;
    TIM2_EGR = 1u;
    TIM2_CR1 = (1u << 7u) | 1u;

    // ADC1 (ADC clock 8 MHz / 2): power on, calibrate, then convert IN1 continuously.
    ADC1_CR2 = 1u;                          // ADON
    for (volatile int i = 0; i < 20; ++i) { // tSTAB: 1 us
    }
    ADC1_CR2 = 1u | (1u << 2u);             // CAL
    while (ADC1_CR2 & (1u << 2u)) {
    }
    ADC1_SQR3 = 1u;                         // SQ1 = IN1
    ADC1_SMPR2 = 7u << 3u;                  // SMP1 = 239.5 cycles
    ADC1_CR1 = 1u << 5u;                    // EOCIE
    ADC1_CR2 = 1u | (1u << 1u);             // CONT (no start yet: another bit changed)
    ADC1_CR2 = 1u | (1u << 1u);             // ADON again: start
    NVIC_ISER0 = 1u << 18u;

    SYST_RVR = 8000u - 1u;                  // 1 ms
    SYST_CVR = 0u;
    SYST_CSR = 7u;

    put_string("ADC to PWM: turn the potentiometer on PA1\r\n");
    for (;;) {
        if (report_due) {
            report_due = 0;
            const uint32_t code = adc_code;
            put_string("ADC ");
            put_number(code);
            put_string(" = ");
            put_number(code * 3300u / 4095u);
            put_string(" mV\r\n");
        }
        __asm volatile("wfi");
    }
}
