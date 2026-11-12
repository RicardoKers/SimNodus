// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// STM32F407 station (experiment E-12, SN-298): the firmware of the board
// comparison of gate G12c.7.
//   168 MHz from the 25 MHz crystal through the PLL (as f4_clock); PCLK1
//   42 MHz, PCLK2 84 MHz.
//   The RTC (on the LSE, or on the LSI without its crystal) wakes the core
//   every second; the core sleeps (WFI) in between.
//   Every second: the LM75 at 0x48 on I2C1 (PB6, PB7) gives the
//   temperature; TIM3 channel 1 on PA6 (1 kHz) gets a duty cycle of ten
//   counts of 1000 per degree; DAC channel 1 on PA4 gets forty codes per
//   degree; USART1 (PA9, 115200 baud) prints through DMA2 a line such as
//   "12:00:01 2026-11-12 lm75 23.5 pwm 235 dac 940".
#define F4_RTC_WKUP RTC_WKUP_IRQHandler
#include "../f4_common/f4_peripherals.h"

#define HSE_VALUE 25000000u

#define TIM3_BASE 0x40000400u
#define TIM_CR1(base) REG((base) + 0x00u)
#define TIM_EGR(base) REG((base) + 0x14u)
#define TIM_CCMR1(base) REG((base) + 0x18u)
#define TIM_CCER(base) REG((base) + 0x20u)
#define TIM_PSC(base) REG((base) + 0x28u)
#define TIM_ARR(base) REG((base) + 0x2Cu)
#define TIM_CCR1(base) REG((base) + 0x34u)
#define DAC_CR REG(0x40007400u)
#define DAC_DHR12R1 REG(0x40007408u)

#define LM75 0x48u

static volatile uint32_t ticks;

void RTC_WKUP_IRQHandler(void)
{
    rtc_clear(1u << 10); // WUTF
    EXTI_PR = 1u << 22;
    ++ticks;
}

static void clock_168mhz(void)
{
    RCC_CR |= 1u << 16; // HSEON
    while (!(RCC_CR & (1u << 17))) { // HSERDY
    }
    FLASH_ACR = 5u | (1u << 8) | (1u << 9) | (1u << 10); // 5 wait states, prefetch, caches
    RCC_PLLCFGR = (HSE_VALUE / 1000000u) | (336u << 6) | (0u << 16) | (1u << 22) | (7u << 24);
    RCC_CR |= 1u << 24; // PLLON
    while (!(RCC_CR & (1u << 25))) {
    }
    RCC_CFGR = (5u << 10) | (4u << 13); // PCLK1 = HCLK / 4, PCLK2 = HCLK / 2
    RCC_CFGR |= 2u;
    while (((RCC_CFGR >> 2) & 3u) != 2u) {
    }
}

int main(void)
{
    clock_168mhz();
    RCC_AHB1ENR |= 1u << 0;                // GPIOAEN
    RCC_APB1ENR |= (1u << 1) | (1u << 29); // TIM3EN, DACEN
    usart1_dma_open(729);                  // 84 MHz / 115200
    i2c1_open(42);

    // PA6: TIM3 channel 1 (function 2), PWM mode 1 at 1 kHz (84 MHz / 84 / 1000).
    GPIO_AFRL(PORT_A) = (GPIO_AFRL(PORT_A) & ~(15u << 24)) | (2u << 24);
    GPIO_MODER(PORT_A) = (GPIO_MODER(PORT_A) & ~((3u << 12) | (3u << 8))) | (2u << 12) | (3u << 8); // PA4 analog
    TIM_PSC(TIM3_BASE) = 83u;
    TIM_ARR(TIM3_BASE) = 999u;
    TIM_CCMR1(TIM3_BASE) = (6u << 4) | (1u << 3);
    TIM_CCER(TIM3_BASE) = 1u;
    TIM_EGR(TIM3_BASE) = 1u;
    TIM_CR1(TIM3_BASE) = (1u << 7) | 1u;
    DAC_CR = 1u; // channel 1 with its buffer, no trigger

    struct line l;
    l.length = 0; // no initializer: without a C library there is no memset
    line_text(&l, "f4_station\r\n");
    line_send(&l);
    rtc_open(0x120000u, 0x00269112u); // 12:00:00, Thursday 2026-11-12
    RTC_WPR = 0xCAu;
    RTC_WPR = 0x53u;
    RTC_CR &= ~(1u << 10);
    while (!(RTC_ISR & 4u)) { // WUTWF
    }
    RTC_WUTR = 0;
    rtc_clear(1u << 10);
    RTC_CR = (RTC_CR & ~7u) | 4u | (1u << 10) | (1u << 14); // every second: ck_spre, WUTE, WUTIE
    RTC_WPR = 0xFFu;
    EXTI_IMR |= 1u << 22;
    EXTI_RTSR |= 1u << 22;
    NVIC_ISER0 = 1u << 3;

    uint32_t done = 0;
    for (;;) {
        __asm__ volatile("wfi");
        if (ticks == done) {
            continue;
        }
        done = ticks;
        uint8_t raw[2];
        line_time(&l);
        if (i2c1_read_register(LM75, 0, raw, 2) != 0) {
            line_text(&l, " lm75: no answer\r\n");
            line_send(&l);
            continue;
        }
        const int32_t halves = (int16_t)(uint16_t)((raw[0] << 8) | raw[1]) / 128;
        const uint32_t tenths = halves > 0 ? (uint32_t)halves * 5u : 0u; // below 0 the outputs stay at 0
        const uint32_t duty = tenths > 1000u ? 1000u : tenths;
        const uint32_t code = tenths * 4u > 4095u ? 4095u : tenths * 4u;
        TIM_CCR1(TIM3_BASE) = duty;
        DAC_DHR12R1 = code;
        line_text(&l, halves < 0 ? " lm75 -" : " lm75 ");
        const uint32_t magnitude = (uint32_t)(halves < 0 ? -halves : halves);
        line_decimal(&l, magnitude / 2u, 1);
        line_text(&l, (magnitude & 1u) ? ".5" : ".0");
        line_text(&l, " pwm ");
        line_decimal(&l, duty, 1);
        line_text(&l, " dac ");
        line_decimal(&l, code, 1);
        line_text(&l, "\r\n");
        line_send(&l);
    }
}
