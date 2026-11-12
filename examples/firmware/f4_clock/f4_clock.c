// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// STM32F407 clock tree (experiment E-12): the HSE crystal, the PLL at
// 168 MHz, the flash latency, and USART1 at 115200 baud on PA9.
//
// HSE_VALUE is the crystal the firmware was written for (25 MHz, as on the
// owner's test board). With another crystal on the diagram or on the board
// the PLL gives another frequency and the baud rate is wrong, as it must be.
//
// Prints once "SYSCLK <Hz> HCLK <Hz> PCLK1 <Hz> PCLK2 <Hz>", computed from
// the registers, then "tick <n>" every second; PC13 toggles every 500 ms.
#define F4_SYSTICK SysTick_Handler
#include "../f4_common/f4.h"

#define HSE_VALUE 25000000u

static volatile uint32_t milliseconds;

void SysTick_Handler(void)
{
    const uint32_t now = ++milliseconds;
    if (now % 500u == 0u) {
        GPIO_ODR(PORT_C) ^= 1u << 13;
    }
}

static uint32_t sysclk_hz(void)
{
    const uint32_t source = (RCC_CFGR >> 2) & 3u;
    if (source == 0u) {
        return 16000000u;
    }
    if (source == 1u) {
        return HSE_VALUE;
    }
    const uint32_t pll = RCC_PLLCFGR;
    const uint32_t input = (pll & (1u << 22)) ? HSE_VALUE : 16000000u;
    const uint32_t m = pll & 0x3Fu;
    const uint32_t n = (pll >> 6) & 0x1FFu;
    const uint32_t p = (((pll >> 16) & 3u) + 1u) * 2u;
    return input / m * n / p;
}

static uint32_t divided(uint32_t hz, uint32_t prescaler_bits)
{
    return prescaler_bits < 4u ? hz : hz >> (prescaler_bits - 3u);
}

int main(void)
{
    RCC_AHB1ENR |= 1u << 2; // GPIOCEN
    GPIO_MODER(PORT_C) = (GPIO_MODER(PORT_C) & ~(3u << 26)) | (1u << 26);

    RCC_CR |= 1u << 16; // HSEON
    while (!(RCC_CR & (1u << 17))) { // HSERDY: waits for ever without a crystal
    }
    FLASH_ACR = 5u | (1u << 8) | (1u << 9) | (1u << 10); // 5 wait states, prefetch, instruction and data caches
    // PLL: 25 MHz / 25 * 336 / 2 = 168 MHz; / 7 = 48 MHz for USB.
    RCC_PLLCFGR = (HSE_VALUE / 1000000u) | (336u << 6) | (0u << 16) | (1u << 22) | (7u << 24);
    RCC_CR |= 1u << 24; // PLLON
    while (!(RCC_CR & (1u << 25))) {
    }
    RCC_CFGR = (5u << 10) | (4u << 13); // PCLK1 = HCLK / 4 (42 MHz), PCLK2 = HCLK / 2 (84 MHz)
    RCC_CFGR |= 2u;                      // SW: the PLL
    while (((RCC_CFGR >> 2) & 3u) != 2u) {
    }

    const uint32_t sysclk = sysclk_hz();
    const uint32_t hpre = (RCC_CFGR >> 4) & 15u;
    const uint32_t hclk = hpre < 8u ? sysclk : sysclk >> (hpre < 12u ? hpre - 7u : hpre - 6u);
    const uint32_t pclk1 = divided(hclk, (RCC_CFGR >> 10) & 7u);
    const uint32_t pclk2 = divided(hclk, (RCC_CFGR >> 13) & 7u);

    SYST_RVR = hclk / 1000u - 1u;
    SYST_CVR = 0;
    SYST_CSR = 7u;
    usart1_start((pclk2 + 115200u / 2u) / 115200u);
    put_text("SYSCLK ");
    put_decimal(sysclk);
    put_text(" HCLK ");
    put_decimal(hclk);
    put_text(" PCLK1 ");
    put_decimal(pclk1);
    put_text(" PCLK2 ");
    put_decimal(pclk2);
    put_text("\r\n");
    for (uint32_t second = 1;; ++second) {
        while (milliseconds < second * 1000u) {
        }
        put_text("tick ");
        put_decimal(second);
        put_text("\r\n");
    }
}
