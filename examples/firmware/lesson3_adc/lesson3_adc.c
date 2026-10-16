// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// Lesson 3 - ADC and UART (examples/lessons/lesson3_adc_uart.md).
// Blue Pill without libraries, internal 8 MHz clock; runs on a real board too.
//   PA2 (ADC1 IN2): potentiometer wiper, a voltage between 0 and 3.3 V.
//   PA0 (TIM2_CH1): PWM 1 kHz whose duty follows the potentiometer.
//   PA1 (ADC1 IN1): the PWM after a 10 kOhm / 1 uF RC filter.
//   Every 200 ms the firmware converts IN2, then IN1 (single conversions,
//   polling EOC), sets the duty and prints on USART1 (PA9, 115200 baud):
//     POT 2048 = 1650 mV  duty 50.0 %  RC 1650 mV
//   PC13 (on-board LED, lit when low) toggles with every line.
#include <stdint.h>

#define REG(address) (*(volatile uint32_t *)(address))
#define RCC_APB2ENR REG(0x40021018u)
#define RCC_APB1ENR REG(0x4002101Cu)
#define GPIOA_CRL REG(0x40010800u)
#define GPIOA_CRH REG(0x40010804u)
#define GPIOC_CRH REG(0x40011004u)
#define GPIOC_ODR REG(0x4001100Cu)
#define USART1_SR REG(0x40013800u)
#define USART1_DR REG(0x40013804u)
#define USART1_BRR REG(0x40013808u)
#define USART1_CR1 REG(0x4001380Cu)
#define ADC1_SR REG(0x40012400u)
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
#define SCB_VTOR REG(0xE000ED08u)

#define ADC_CR2_ADON (1u << 0u)
#define ADC_CR2_CAL (1u << 2u)
#define ADC_CR2_EXTSEL_SWSTART (7u << 17u)
#define ADC_CR2_EXTTRIG (1u << 20u)
#define ADC_CR2_SWSTART (1u << 22u)
#define ADC_SR_EOC (1u << 1u)

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

static void put_char(char c)
{
    while (!(USART1_SR & (1u << 7u))) { // TXE
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

// One conversion of `channel`: select it, start, wait for EOC, read DR.
// 239.5 + 12.5 ADC clock cycles at 4 MHz: 63 us.
static uint32_t adc_read(uint32_t channel)
{
    ADC1_SQR3 = channel;
    ADC1_CR2 |= ADC_CR2_SWSTART;
    while (!(ADC1_SR & ADC_SR_EOC)) {
    }
    return ADC1_DR & 0xFFFu; // reading DR clears EOC
}

static uint32_t millivolts(uint32_t code)
{
    return (code * 3300u + 2048u) / 4096u; // 1 LSB = 3.3 V / 4096 = 0.806 mV
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

    // PA0: alternate-function push-pull (0xA); PA1, PA2: analog (0x0);
    // PA9: alternate-function push-pull 50 MHz (0xB); PA10: floating input (0x4).
    GPIOA_CRL = (GPIOA_CRL & ~0xFFFu) | 0x00Au;
    GPIOA_CRH = (GPIOA_CRH & ~0xFF0u) | (0xBu << 4u) | (0x4u << 8u);
    GPIOC_CRH = (GPIOC_CRH & ~(0xFu << 20u)) | (0x2u << 20u);

    USART1_BRR = 69u;                      // 8 MHz / 115200
    USART1_CR1 = (1u << 13u) | (1u << 3u); // UE, TE

    // TIM2 CH1: 1 MHz counter, 1000 counts = 1 kHz, PWM mode 1 with preload.
    TIM2_PSC = 7u;
    TIM2_ARR = 999u;
    TIM2_CCMR1 = (6u << 4u) | (1u << 3u);
    TIM2_CCER = 1u;
    TIM2_EGR = 1u;
    TIM2_CR1 = (1u << 7u) | 1u;

    // ADC1: ADC clock 8 MHz / 2 = 4 MHz (the reset value of ADCPRE). Power on,
    // wait 1 us, calibrate, longest sample time on IN1 and IN2, software start.
    ADC1_CR2 = ADC_CR2_ADON;
    for (volatile int i = 0; i < 20; ++i) {
    }
    ADC1_CR2 |= ADC_CR2_CAL;
    while (ADC1_CR2 & ADC_CR2_CAL) {
    }
    ADC1_SMPR2 = (7u << 3u) | (7u << 6u); // SMP1, SMP2 = 239.5 cycles
    ADC1_CR2 = ADC_CR2_ADON | ADC_CR2_EXTSEL_SWSTART | ADC_CR2_EXTTRIG;

    SYST_RVR = 8000u - 1u; // 1 ms
    SYST_CVR = 0u;
    SYST_CSR = 7u;

    put_string("Lesson 3: ADC and UART\r\n");
    uint32_t next = 0;
    for (;;) {
        while ((int32_t)(milliseconds - next) < 0) {
            __asm volatile("wfi");
        }
        next += 200u;
        const uint32_t pot = adc_read(2u);
        const uint32_t rc = adc_read(1u);
        const uint32_t duty = pot * 1000u / 4096u; // per mille, 0..999
        TIM2_CCR1 = duty;
        put_string("POT ");
        put_number(pot);
        put_string(" = ");
        put_number(millivolts(pot));
        put_string(" mV  duty ");
        put_number(duty / 10u);
        put_char('.');
        put_number(duty % 10u);
        put_string(" %  RC ");
        put_number(millivolts(rc));
        put_string(" mV\r\n");
        GPIOC_ODR ^= 1u << 13u;
    }
}
