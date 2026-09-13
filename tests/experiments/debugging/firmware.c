// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "mailbox.h"

#define REG(address) (*(volatile uint32_t *)(address))
#define GPIOA_CRL REG(0x40010800u)
#define GPIOA_BSRR REG(0x40010810u)
#define ADC_SR REG(0x40012400u)
#define ADC_CR2 REG(0x40012408u)
#define ADC_SQR3 REG(0x40012434u)
#define ADC_DR REG(0x4001244cu)
#define ADC_EOC (1u << 1u)
#define ADC_ADON (1u << 0u)
#define ADC_EXTTRIG (1u << 20u)
#define ADC_SWSTART (1u << 22u)
#define SYSTICK_CTRL REG(0xe000e010u)
#define SYSTICK_LOAD REG(0xe000e014u)
#define SYSTICK_VAL REG(0xe000e018u)

volatile sn_e05_mailbox mailbox __attribute__((section(".mailbox")));
extern uint32_t _data_load, _data_start, _data_end, _bss_start, _bss_end, _stack_top;

void Reset_Handler(void);
void Default_Handler(void);
void SysTick_Handler(void);

__attribute__((section(".vectors"), used))
const uintptr_t vectors[] = {
    (uintptr_t)&_stack_top, (uintptr_t)Reset_Handler,
    (uintptr_t)Default_Handler, (uintptr_t)Default_Handler,
    (uintptr_t)Default_Handler, (uintptr_t)Default_Handler,
    (uintptr_t)Default_Handler, 0, 0, 0, 0,
    (uintptr_t)Default_Handler, (uintptr_t)Default_Handler, 0,
    (uintptr_t)Default_Handler, (uintptr_t)SysTick_Handler
};

void Default_Handler(void)
{
    mailbox.fault = 1;
    for (;;) { __asm volatile("wfi"); }
}

__attribute__((noinline, used))
void gpio_change_marker(uint32_t high)
{
    GPIOA_BSRR = high ? 1u : (1u << 16u);
    mailbox.gpio_changes++;
}

__attribute__((noinline, used))
static uint32_t step_helper(uint32_t value)
{
    return value + 1u;
}

__attribute__((noinline, used))
void step_target(void)
{
    uint32_t value = mailbox.step_count;
    value = step_helper(value);
    mailbox.step_count = value;
}

__attribute__((noinline, used))
uint32_t adc_read_marker(void)
{
    ADC_SQR3 = 0;
    ADC_CR2 = ADC_ADON | ADC_EXTTRIG;
    ADC_CR2 = ADC_ADON | ADC_EXTTRIG | ADC_SWSTART;
    uint32_t timeout = 500000u;
    while (!(ADC_SR & ADC_EOC) && --timeout) { }
    if (!timeout) {
        mailbox.fault = 2;
        return 0xffffffffu;
    }
    mailbox.adc_reads++;
    return ADC_DR;
}

void SysTick_Handler(void)
{
    const uint32_t tick = ++mailbox.ticks;
    if (tick == 2u) {
        gpio_change_marker(1u);
        step_target();
    } else if (tick == 4u) {
        mailbox.adc_result = adc_read_marker();
    } else if (tick == 6u) {
        gpio_change_marker(0u);
    }
}

void Reset_Handler(void)
{
    uint32_t *source = &_data_load;
    for (uint32_t *target = &_data_start; target < &_data_end;) { *target++ = *source++; }
    for (uint32_t *target = &_bss_start; target < &_bss_end;) { *target++ = 0; }
    volatile uint32_t *word = (volatile uint32_t *)&mailbox;
    for (uint32_t index = 0; index < sizeof(mailbox) / sizeof(uint32_t); ++index) { word[index] = 0; }

    REG(0xe000ed08u) = 0x08000000u;
    REG(0x40021018u) = (1u << 2u) | (1u << 9u);
    GPIOA_CRL = (GPIOA_CRL & ~0xfu) | 0x2u;
    ADC_CR2 = ADC_ADON;
    mailbox.magic = SN_E05_BOOT_MAGIC;
    SYSTICK_LOAD = 7999u;
    SYSTICK_VAL = 0u;
    SYSTICK_CTRL = 7u;
    for (;;) { __asm volatile("wfi"); }
}
