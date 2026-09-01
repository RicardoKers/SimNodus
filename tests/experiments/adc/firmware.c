// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "mailbox.h"

#define REG(address) (*(volatile uint32_t *)(address))
#define ADC_SR REG(0x40012400u)
#define ADC_CR2 REG(0x40012408u)
#define ADC_SMPR1 REG(0x4001240cu)
#define ADC_SMPR2 REG(0x40012410u)
#define ADC_SQR1 REG(0x4001242cu)
#define ADC_SQR3 REG(0x40012434u)
#define ADC_DR REG(0x4001244cu)
#define ADC_EOC (1u << 1u)
#define ADC_ADON (1u << 0u)
#define ADC_EXTTRIG (1u << 20u)
#define ADC_SWSTART (1u << 22u)

volatile sn_e04_mailbox mailbox __attribute__((section(".mailbox")));
extern uint32_t _data_load, _data_start, _data_end, _bss_start, _bss_end, _stack_top;

void Reset_Handler(void);
void Default_Handler(void);

__attribute__((section(".vectors"), used))
const uintptr_t vectors[] = {
    (uintptr_t)&_stack_top, (uintptr_t)Reset_Handler,
    (uintptr_t)Default_Handler, (uintptr_t)Default_Handler,
    (uintptr_t)Default_Handler, (uintptr_t)Default_Handler,
    (uintptr_t)Default_Handler, 0, 0, 0, 0,
    (uintptr_t)Default_Handler, (uintptr_t)Default_Handler, 0,
    (uintptr_t)Default_Handler, (uintptr_t)Default_Handler
};

void Default_Handler(void)
{
    mailbox.fault = 1;
    for (;;) { __asm volatile("wfi"); }
}

void Reset_Handler(void)
{
    uint32_t *source = &_data_load;
    for (uint32_t *target = &_data_start; target < &_data_end;) { *target++ = *source++; }
    for (uint32_t *target = &_bss_start; target < &_bss_end;) { *target++ = 0; }
    volatile uint32_t *word = (volatile uint32_t *)&mailbox;
    for (uint32_t i = 0; i < sizeof(mailbox) / sizeof(uint32_t); ++i) { word[i] = 0; }

    REG(0xe000ed08u) = 0x08000000u;
    REG(0x40021018u) = (1u << 2u) | (1u << 9u); // GPIOA and ADC1; storage-only RCC profile.
    REG(0x40010800u) = (REG(0x40010800u) & ~0xffu); // PA0 and PA1 analog input mode.
    ADC_SMPR1 = 0;
    ADC_SMPR2 = 0;
    ADC_SQR1 = 0;
    ADC_SQR3 = 0;
    ADC_CR2 = ADC_ADON;
    mailbox.magic = SN_E04_BOOT_MAGIC;

    for (;;) {
        const uint32_t command = mailbox.command;
        if (command == mailbox.acknowledged) {
            continue;
        }

        const uint32_t channel = mailbox.channel & 0x1fu;
        const uint32_t sample = mailbox.sample_setting & 7u;
        ADC_SQR3 = channel;
        if (channel < 10u) {
            const uint32_t shift = channel * 3u;
            ADC_SMPR2 = (ADC_SMPR2 & ~(7u << shift)) | (sample << shift);
        }
        ADC_CR2 = ADC_ADON | ADC_EXTTRIG;
        ADC_CR2 = ADC_ADON | ADC_EXTTRIG | ADC_SWSTART;
        mailbox.started = command;

        uint32_t timeout = 500000u;
        while (!(ADC_SR & ADC_EOC) && --timeout) { }
        if (!timeout) {
            mailbox.fault = 2;
            mailbox.acknowledged = command;
            continue;
        }
        mailbox.status_before_read = ADC_SR;
        mailbox.result = ADC_DR;
        mailbox.completed = command;
        mailbox.acknowledged = command;
    }
}
