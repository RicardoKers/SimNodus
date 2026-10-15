// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// Lesson 2 - Button input and EXTI (examples/lessons/lesson2_button_exti.md).
// Blue Pill without libraries, internal 8 MHz clock; runs on a real board too.
//   PA1: input from a push button to ground with an external 10 kOhm pull-up
//        (the internal pull-up is not used, so the circuit decides the level).
//   EXTI1 (falling edge of PA1): counts the edge, toggles the LED on PA0 and
//        prints the count on USART1 (PA9, 115200 baud on a real board).
//   A bouncing button gives several edges per press: the LED may end up
//   unchanged and the count jumps. An RC filter across the button fixes it.
#include <stdint.h>

#define REG(address) (*(volatile uint32_t *)(address))
#define RCC_APB2ENR REG(0x40021018u)
#define AFIO_EXTICR1 REG(0x40010008u)
#define GPIOA_CRL REG(0x40010800u)
#define GPIOA_CRH REG(0x40010804u)
#define GPIOA_ODR REG(0x4001080Cu)
#define GPIOA_BSRR REG(0x40010810u)
#define USART1_SR REG(0x40013800u)
#define USART1_DR REG(0x40013804u)
#define USART1_BRR REG(0x40013808u)
#define USART1_CR1 REG(0x4001380Cu)
#define EXTI_IMR REG(0x40010400u)
#define EXTI_FTSR REG(0x4001040Cu)
#define EXTI_PR REG(0x40010414u)
#define NVIC_ISER0 REG(0xE000E100u)
#define SCB_VTOR REG(0xE000ED08u)

extern uint32_t _data_load, _data_start, _data_end, _bss_start, _bss_end, _stack_top;

void Reset_Handler(void);
void Default_Handler(void);
void EXTI1_IRQHandler(void);

__attribute__((section(".vectors"), used))
const uintptr_t vectors[16 + 8] = {
    [0] = (uintptr_t)&_stack_top,
    [1] = (uintptr_t)Reset_Handler,
    [2 ... 6] = (uintptr_t)Default_Handler,
    [11] = (uintptr_t)Default_Handler,
    [12] = (uintptr_t)Default_Handler,
    [14 ... 22] = (uintptr_t)Default_Handler,
    [16 + 7] = (uintptr_t)EXTI1_IRQHandler,
};

static volatile uint32_t edges;
static volatile uint32_t pending; // edges not printed yet

void Default_Handler(void)
{
    for (;;) {
    }
}

void EXTI1_IRQHandler(void)
{
    if (EXTI_PR & (1u << 1u)) {
        EXTI_PR = 1u << 1u; // write one to clear
        ++edges;
        ++pending;
        GPIOA_ODR ^= 1u; // toggle the LED on PA0
    }
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
    RCC_APB2ENR |= (1u << 0u) | (1u << 2u) | (1u << 14u); // AFIOEN, IOPAEN, USART1EN
    GPIOA_CRL = (GPIOA_CRL & ~0xFFu) | 0x42u;              // PA0 output; PA1 floating input
    GPIOA_BSRR = 1u << 16u;                                // LED off
    GPIOA_CRH = (GPIOA_CRH & ~0xF0u) | (0xBu << 4u);       // PA9: USART1 TX
    USART1_BRR = 69u;                                      // 8 MHz / 115200
    USART1_CR1 = (1u << 13u) | (1u << 3u);                 // UE, TE
    AFIO_EXTICR1 &= ~(0xFu << 4u);                         // EXTI1 from port A
    EXTI_FTSR |= 1u << 1u;
    EXTI_PR = 1u << 1u;
    EXTI_IMR |= 1u << 1u;
    NVIC_ISER0 = 1u << 7u;
    put_string("Lesson 2: press the button. Each falling edge is counted.\r\n");
    for (;;) {
        if (pending) {
            __asm volatile("cpsid i");
            pending = 0;
            const uint32_t total = edges;
            __asm volatile("cpsie i");
            put_string("edges: ");
            put_number(total);
            put_string(", LED ");
            put_string((GPIOA_ODR & 1u) ? "on\r\n" : "off\r\n");
        }
        __asm volatile("wfi");
    }
}
