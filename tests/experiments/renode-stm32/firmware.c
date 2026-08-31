// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "mailbox.h"

#define REG(address) (*(volatile uint32_t *)(address))
#define GPIO_CRL REG(0x40010800u)
#define GPIO_IDR REG(0x40010808u)
#define GPIO_BSRR REG(0x40010810u)
#define GPIO_BRR REG(0x40010814u)
#define EXTI_PR REG(0x40010414u)

volatile sn_mailbox mailbox __attribute__((section(".mailbox")));
volatile uint32_t initialized_cookie = SN_DATA_COOKIE;
volatile uint32_t zero_cookie;
extern uint32_t _data_load, _data_start, _data_end, _bss_start, _bss_end, _stack_top;

void Reset_Handler(void);
void Default_Handler(void);
void SysTick_Handler(void);
void EXTI1_Handler(void);

// Core vectors plus IRQ0..7. Unused entries trap instead of executing address zero.
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
    if (tick & 1u) { GPIO_BSRR = 1u; } else { GPIO_BRR = 1u; }
    const uint32_t input = (GPIO_IDR >> 1u) & 1u;
    mailbox.input = input;
    if (input) { GPIO_BSRR = 4u; } else { GPIO_BRR = 4u; }
}

void EXTI1_Handler(void)
{
    if (EXTI_PR & 2u) {
        EXTI_PR = 2u; // Write one to clear before returning from the interrupt.
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
    REG(0xe000ed08u) = 0x08000000u; // VTOR: flash vector table.
    REG(0x40021018u) = 5u; // AFIO and GPIOA enable; profile stores but does not model RCC.
    GPIO_CRL = 0x44424242u; // PA0/2/4: 2 MHz push-pull; PA1/3: floating input.
    GPIO_BRR = 0x15u;
    REG(0x40010400u) = 2u; // EXTI1 interrupt mask.
    REG(0x40010408u) = 2u; // Rising edge.
    REG(0x4001040cu) = 2u; // Falling edge.
    EXTI_PR = 2u;
    REG(0xe000e100u) = 1u << 7u; // NVIC IRQ7 is EXTI1.
    REG(0xe000e014u) = 7999u; // 8000 processor ticks at fixed 8 MHz.
    REG(0xe000e018u) = 0;
    REG(0xe000e010u) = 7u; // ENABLE, TICKINT, processor CLKSOURCE.
    mailbox.systick_reload = REG(0xe000e014u);
    mailbox.systick_control = REG(0xe000e010u);
    mailbox.magic = SN_BOOT_MAGIC;
    for (;;) {
        const uint32_t command = mailbox.command;
        if (command != mailbox.acknowledged) {
            GPIO_CRL = (GPIO_CRL & ~(15u << 12u)) | ((mailbox.mode & 15u) << 12u);
            if (mailbox.drive) { GPIO_BSRR = 8u; } else { GPIO_BRR = 8u; }
            mailbox.mode_readback = GPIO_CRL;
            mailbox.acknowledged = command;
        }
        __asm volatile("wfi");
    }
}
