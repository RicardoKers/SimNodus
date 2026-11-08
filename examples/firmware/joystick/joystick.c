// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// A two-axis joystick module read without libraries, for SimNodus and for a
// real Blue Pill (internal 8 MHz clock). The module is powered from 3.3 V:
// the Blue Pill's analog inputs do not take 5 V.
//   PA0 (ADC1 IN0): VRx; PA1 (ADC1 IN1): VRy. One conversion of each every
//   20 ms, 239.5-cycle sample time.
//   PB0: SW with the internal pull-up (low while the stick is pushed).
//   PC13 (on-board LED, lit when low): lit while the stick is pushed.
//   USART1 (PA9 TX, 115200 baud): "X <code> Y <code> SW <0|1> <direction>"
//   every 200 ms; the direction is "center", or "left"/"right" and
//   "up"/"down" when an axis is more than a quarter of its range off center.
#include <stdint.h>

#define REG(address) (*(volatile uint32_t *)(address))
#define RCC_APB2ENR REG(0x40021018u)
#define GPIOA_CRL REG(0x40010800u)
#define GPIOA_CRH REG(0x40010804u)
#define GPIOB_CRL REG(0x40010C00u)
#define GPIOB_IDR REG(0x40010C08u)
#define GPIOB_ODR REG(0x40010C0Cu)
#define GPIOC_CRH REG(0x40011004u)
#define GPIOC_BSRR REG(0x40011010u)
#define USART1_SR REG(0x40013800u)
#define USART1_DR REG(0x40013804u)
#define USART1_BRR REG(0x40013808u)
#define USART1_CR1 REG(0x4001380Cu)
#define ADC1_SR REG(0x40012400u)
#define ADC1_CR2 REG(0x40012408u)
#define ADC1_SMPR2 REG(0x40012410u)
#define ADC1_SQR3 REG(0x40012434u)
#define ADC1_DR REG(0x4001244Cu)
#define SYST_CSR REG(0xE000E010u)
#define SYST_RVR REG(0xE000E014u)
#define SYST_CVR REG(0xE000E018u)
#define SCB_VTOR REG(0xE000ED08u)

#define SR_TXE (1u << 7u)
#define SR_EOC (1u << 1u)

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

// One conversion of `channel`: ADON written again starts it (no external trigger).
static uint32_t convert(uint32_t channel)
{
    ADC1_SQR3 = channel;
    ADC1_CR2 = 1u;
    while (!(ADC1_SR & SR_EOC)) {
    }
    return ADC1_DR & 0xFFFu; // reading DR clears EOC
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
    // AFIOEN, IOPAEN, IOPBEN, IOPCEN, ADC1EN, USART1EN.
    RCC_APB2ENR |= (1u << 0u) | (1u << 2u) | (1u << 3u) | (1u << 4u) | (1u << 9u) | (1u << 14u);

    // PA0, PA1: analog (0x0); PA9: alternate-function push-pull 50 MHz (0xB);
    // PB0: input with pull-up (0x8, ODR bit set); PC13: push-pull output 2 MHz.
    GPIOA_CRL &= ~0xFFu;
    GPIOA_CRH = (GPIOA_CRH & ~0xF0u) | (0xBu << 4u);
    GPIOB_CRL = (GPIOB_CRL & ~0xFu) | 0x8u;
    GPIOB_ODR |= 1u;
    GPIOC_CRH = (GPIOC_CRH & ~(0xFu << 20u)) | (0x2u << 20u);
    GPIOC_BSRR = 1u << 13u;

    USART1_BRR = 69u;                       // 8 MHz / 115200
    USART1_CR1 = (1u << 13u) | (1u << 3u);  // UE, TE

    // ADC1 (ADC clock 8 MHz / 2): power on, calibrate.
    ADC1_CR2 = 1u;                          // ADON
    for (volatile int i = 0; i < 20; ++i) { // tSTAB: 1 us
    }
    ADC1_CR2 = 1u | (1u << 2u);             // CAL
    while (ADC1_CR2 & (1u << 2u)) {
    }
    ADC1_SMPR2 = (7u << 0u) | (7u << 3u);   // SMP0, SMP1 = 239.5 cycles

    SYST_RVR = 8000u - 1u;                  // 1 ms
    SYST_CVR = 0u;
    SYST_CSR = 7u;

    put_string("Joystick: drag the stick, click SW\r\n");
    uint32_t next_read = 0;
    uint32_t next_report = 0;
    uint32_t x = 2048u;
    uint32_t y = 2048u;
    for (;;) {
        const uint32_t now = milliseconds;
        const int pushed = (GPIOB_IDR & 1u) == 0u;
        GPIOC_BSRR = pushed ? (1u << (13u + 16u)) : (1u << 13u);
        if ((int32_t)(now - next_read) >= 0) {
            next_read = now + 20u;
            x = convert(0u);
            y = convert(1u);
        }
        if ((int32_t)(now - next_report) >= 0) {
            next_report = now + 200u;
            put_string("X ");
            put_number(x);
            put_string(" Y ");
            put_number(y);
            put_string(pushed ? " SW 1 " : " SW 0 ");
            const int left = x < 1024u, right = x > 3072u, down = y < 1024u, up = y > 3072u;
            if (!left && !right && !up && !down) {
                put_string("center");
            }
            put_string(left ? "left" : right ? "right" : "");
            put_string((left || right) && (up || down) ? " " : "");
            put_string(up ? "up" : down ? "down" : "");
            put_string("\r\n");
        }
        __asm volatile("wfi");
    }
}
