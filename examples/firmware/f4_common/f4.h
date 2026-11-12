// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// What the STM32F407 test firmware of experiment E-12 shares: register
// addresses (RM0090), the vector table and the start-up code. No library:
// each firmware is one C file that includes this header once.
#ifndef SIMNODUS_F4_H
#define SIMNODUS_F4_H

#include <stdint.h>

#define REG(address) (*(volatile uint32_t *)(address))

#define RCC_CR REG(0x40023800u)
#define RCC_PLLCFGR REG(0x40023804u)
#define RCC_CFGR REG(0x40023808u)
#define RCC_AHB1ENR REG(0x40023830u)
#define RCC_APB1ENR REG(0x40023840u)
#define RCC_APB2ENR REG(0x40023844u)
#define RCC_CSR REG(0x40023874u)
#define FLASH_ACR REG(0x40023C00u)

#define GPIO_BASE(port) (0x40020000u + 0x400u * (uint32_t)(port)) // port 0 = A
#define GPIO_MODER(port) REG(GPIO_BASE(port) + 0x00u)
#define GPIO_OTYPER(port) REG(GPIO_BASE(port) + 0x04u)
#define GPIO_PUPDR(port) REG(GPIO_BASE(port) + 0x0Cu)
#define GPIO_IDR(port) REG(GPIO_BASE(port) + 0x10u)
#define GPIO_ODR(port) REG(GPIO_BASE(port) + 0x14u)
#define GPIO_BSRR(port) REG(GPIO_BASE(port) + 0x18u)
#define GPIO_AFRL(port) REG(GPIO_BASE(port) + 0x20u)
#define GPIO_AFRH(port) REG(GPIO_BASE(port) + 0x24u)
enum { PORT_A, PORT_B, PORT_C, PORT_D, PORT_E };

#define SYSCFG_EXTICR1 REG(0x40013808u)
#define EXTI_IMR REG(0x40013C00u)
#define EXTI_RTSR REG(0x40013C08u)
#define EXTI_FTSR REG(0x40013C0Cu)
#define EXTI_PR REG(0x40013C14u)

#define USART1_SR REG(0x40011000u)
#define USART1_DR REG(0x40011004u)
#define USART1_BRR REG(0x40011008u)
#define USART1_CR1 REG(0x4001100Cu)

#define SYST_CSR REG(0xE000E010u)
#define SYST_RVR REG(0xE000E014u)
#define SYST_CVR REG(0xE000E018u)
#define NVIC_ISER0 REG(0xE000E100u)
#define SCB_VTOR REG(0xE000ED08u)
#define SCB_CPACR REG(0xE000ED88u)
#define DWT_CTRL REG(0xE0001000u)
#define DWT_CYCCNT REG(0xE0001004u)
#define DEMCR REG(0xE000EDFCu)

extern uint32_t _data_load, _data_start, _data_end, _bss_start, _bss_end, _stack_top;

int main(void);
void Reset_Handler(void);
void Default_Handler(void);
// A firmware names the handlers it has before including this header
// (#define F4_SYSTICK SysTick_Handler); the others end in Default_Handler.
#ifndef F4_SYSTICK
#define F4_SYSTICK Default_Handler
#endif
#ifndef F4_EXTI0
#define F4_EXTI0 Default_Handler
#endif
#ifndef F4_USART1
#define F4_USART1 Default_Handler
#endif
void F4_SYSTICK(void);
void F4_EXTI0(void);
void F4_USART1(void);
#ifdef F4_ADC
void F4_ADC(void);
#endif

__attribute__((section(".vectors"), used))
const uintptr_t vectors[16 + 82] = {
    [0] = (uintptr_t)&_stack_top,
    [1] = (uintptr_t)Reset_Handler,
    [2] = (uintptr_t)Default_Handler, // NMI
    [3] = (uintptr_t)Default_Handler, // HardFault
    [4] = (uintptr_t)Default_Handler, // MemManage
    [5] = (uintptr_t)Default_Handler, // BusFault
    [6] = (uintptr_t)Default_Handler, // UsageFault
    [11] = (uintptr_t)Default_Handler, // SVCall
    [14] = (uintptr_t)Default_Handler, // PendSV
    [15] = (uintptr_t)F4_SYSTICK,
    [16 + 6] = (uintptr_t)F4_EXTI0,
#ifdef F4_ADC
    [16 + 18] = (uintptr_t)F4_ADC,
#endif
    [16 + 37] = (uintptr_t)F4_USART1,
};

void Default_Handler(void)
{
    for (;;) {
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
    SCB_CPACR |= 0xFu << 20; // the FPU on for every mode, as STM32CubeIDE's start-up does
    main();
    for (;;) {
    }
}

// USART1 on PA9 (TX, alternate function 7), 8N1; `brr` is PCLK2 / baud.
static inline void usart1_start(uint32_t brr)
{
    RCC_AHB1ENR |= 1u << 0;  // GPIOAEN
    RCC_APB2ENR |= 1u << 4;  // USART1EN
    GPIO_MODER(PORT_A) = (GPIO_MODER(PORT_A) & ~(3u << 18)) | (2u << 18);
    GPIO_AFRH(PORT_A) = (GPIO_AFRH(PORT_A) & ~(15u << 4)) | (7u << 4);
    USART1_BRR = brr;
    USART1_CR1 = (1u << 13) | (1u << 3); // UE, TE
}

static inline void put_char(char c)
{
    while (!(USART1_SR & (1u << 7))) { // TXE
    }
    USART1_DR = (uint32_t)(uint8_t)c;
}

static inline void put_text(const char *text)
{
    while (*text) {
        put_char(*text++);
    }
}

static inline void put_hex(uint32_t value)
{
    for (int shift = 28; shift >= 0; shift -= 4) {
        put_char("0123456789ABCDEF"[(value >> shift) & 15u]);
    }
}

static inline void put_decimal(uint32_t value)
{
    char digits[10];
    int count = 0;
    do {
        digits[count++] = (char)('0' + value % 10u);
        value /= 10u;
    } while (value != 0u);
    while (count > 0) {
        put_char(digits[--count]);
    }
}

// Waits until the last byte has left the pin.
static inline void usart1_flush(void)
{
    while (!(USART1_SR & (1u << 6))) { // TC
    }
}

#endif
