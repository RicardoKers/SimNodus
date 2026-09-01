// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "mailbox.h"

#define REG(address) (*(volatile uint32_t *)(address))
#define GPIO_CRL REG(0x40010800u)
#define GPIO_IDR REG(0x40010808u)
#define GPIO_BSRR REG(0x40010810u)
#define GPIO_BRR REG(0x40010814u)
#define EXTI_PR REG(0x40010414u)

volatile sn_e03_mailbox mailbox __attribute__((section(".mailbox")));
volatile uint32_t initialized_cookie = SN_E03_DATA_COOKIE;
volatile uint32_t zero_cookie;
extern uint32_t _data_load, _data_start, _data_end, _bss_start, _bss_end, _stack_top;

void Reset_Handler(void);
void Default_Handler(void);
void SysTick_Handler(void);
void EXTI1_Handler(void);

__attribute__((section(".vectors"), used))
const uintptr_t vectors[] = {
    (uintptr_t)&_stack_top, (uintptr_t)Reset_Handler,
    (uintptr_t)Default_Handler, (uintptr_t)Default_Handler,
    (uintptr_t)Default_Handler, (uintptr_t)Default_Handler,
    (uintptr_t)Default_Handler, 0, 0, 0, 0,
    (uintptr_t)Default_Handler, (uintptr_t)Default_Handler, 0,
    (uintptr_t)Default_Handler, (uintptr_t)SysTick_Handler,
    (uintptr_t)Default_Handler, (uintptr_t)Default_Handler,
    (uintptr_t)Default_Handler, (uintptr_t)Default_Handler,
    (uintptr_t)Default_Handler, (uintptr_t)Default_Handler,
    (uintptr_t)Default_Handler, (uintptr_t)EXTI1_Handler
};

void Default_Handler(void)
{
    mailbox.fault = 1;
    for (;;) { __asm volatile("wfi"); }
}

void SysTick_Handler(void)
{
    const uint32_t tick = mailbox.ticks + 1u;
    mailbox.ticks = tick;
    if (tick == 1u) { GPIO_BSRR = 1u; }
    if (tick == 4u) { GPIO_BRR = 1u; }
    mailbox.input = (GPIO_IDR >> 1u) & 1u;
    if (mailbox.input) { GPIO_BSRR = 4u; } else { GPIO_BRR = 4u; }
}

void EXTI1_Handler(void)
{
    if (EXTI_PR & 2u) {
        EXTI_PR = 2u;
        mailbox.exti_count++;
        mailbox.exti_input = (GPIO_IDR >> 1u) & 1u;
        if (mailbox.exti_input) { GPIO_BSRR = 16u; } else { GPIO_BRR = 16u; }
    }
}

void Reset_Handler(void)
{
    uint32_t *source = &_data_load;
    for (uint32_t *target = &_data_start; target < &_data_end;) { *target++ = *source++; }
    for (uint32_t *target = &_bss_start; target < &_bss_end;) { *target++ = 0; }
    volatile uint32_t *status = (volatile uint32_t *)&mailbox;
    for (uint32_t i = 0; i < sizeof(mailbox) / sizeof(uint32_t); ++i) { status[i] = 0; }
    mailbox.data_check = initialized_cookie;
    mailbox.bss_check = zero_cookie;
    REG(0xe000ed08u) = 0x08000000u;
    REG(0x40021018u) = 5u;
    GPIO_CRL = 0x44424242u;
    GPIO_BRR = 0x15u;
    REG(0x40010400u) = 2u;
    REG(0x40010408u) = 2u;
    REG(0x4001040cu) = 2u;
    EXTI_PR = 2u;
    REG(0xe000e100u) = 1u << 7u;
    REG(0xe000e014u) = 7999u;
    REG(0xe000e018u) = 0;
    REG(0xe000e010u) = 7u;
    mailbox.magic = SN_E03_BOOT_MAGIC;
    for (;;) { __asm volatile("wfi"); }
}
