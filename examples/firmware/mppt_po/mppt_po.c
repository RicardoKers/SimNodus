// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// Maximum power point tracking by Perturb & Observe, without libraries, for
// SimNodus and for a real Blue Pill (internal 8 MHz clock). The board sets the
// current an electronic load (op-amp, MOSFET and a 1 ohm shunt) draws from a
// solar panel and climbs the power curve:
//   PA0 (TIM2_CH1): PWM 10 kHz; an RC filter and a divider turn its duty into
//   the load's current setpoint (0..0.67 V = 0..0.67 A).
//   PA1 (ADC1 IN1): panel voltage through a 100k/15k divider (x 115/15).
//   PA2 (ADC1 IN2): shunt voltage, 1 mV per mA.
//   Every 50 ms: measure, compare the power with the last one, keep or reverse
//   the direction, move the duty by 1 %.
//   PC13 (on-board LED, lit when low): toggles at each step.
//   USART1 (PA9 TX, 115200 baud): "V <mV> mV I <mA> mA P <mW> mW D <duty> %"
//   every 250 ms.
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

#define SR_TXE (1u << 7u)
#define SR_EOC (1u << 1u)
#define PERIOD 800u // 8 MHz / 800 = 10 kHz

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

// The average of 8 conversions of `channel`, in millivolts at the pin.
static uint32_t millivolts(uint32_t channel)
{
    uint32_t sum = 0;
    ADC1_SQR3 = channel;
    for (int k = 0; k < 8; ++k) {
        ADC1_CR2 = 1u; // ADON again starts a conversion
        while (!(ADC1_SR & SR_EOC)) {
        }
        sum += ADC1_DR & 0xFFFu;
    }
    return sum * 3300u / (8u * 4095u);
}

static void set_duty(uint32_t permille)
{
    TIM2_CCR1 = permille * PERIOD / 1000u;
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

    // PA0: alternate-function push-pull 2 MHz (0xA); PA1, PA2: analog (0x0);
    // PA9: alternate-function push-pull 50 MHz (0xB); PC13: push-pull output.
    GPIOA_CRL = (GPIOA_CRL & ~0xFFFu) | 0x0Au;
    GPIOA_CRH = (GPIOA_CRH & ~0xF0u) | (0xBu << 4u);
    GPIOC_CRH = (GPIOC_CRH & ~(0xFu << 20u)) | (0x2u << 20u);

    USART1_BRR = 69u;                       // 8 MHz / 115200
    USART1_CR1 = (1u << 13u) | (1u << 3u);  // UE, TE

    // TIM2: 10 kHz, CH1 PWM mode 1 with preload.
    TIM2_PSC = 0u;
    TIM2_ARR = PERIOD - 1u;
    TIM2_CCMR1 = (6u << 4u) | (1u << 3u);
    TIM2_CCER = 1u;
    TIM2_EGR = 1u;
    TIM2_CR1 = (1u << 7u) | 1u;

    // ADC1: power on, calibrate, 239.5-cycle sample time on IN1 and IN2.
    ADC1_CR2 = 1u;
    for (volatile int i = 0; i < 20; ++i) {
    }
    ADC1_CR2 = 1u | (1u << 2u);
    while (ADC1_CR2 & (1u << 2u)) {
    }
    ADC1_SMPR2 = (7u << 3u) | (7u << 6u);

    SYST_RVR = 8000u - 1u;                  // 1 ms
    SYST_CVR = 0u;
    SYST_CSR = 7u;

    put_string("MPPT, Perturb & Observe: change the light on the panel\r\n");
    uint32_t duty = 500u;                   // per mille
    int32_t step = 10;                      // 1 % of duty per step
    uint32_t last_power = 0;
    uint32_t volts = 0, amps = 0, power = 0;
    uint32_t next_step = 0, next_report = 0;
    set_duty(duty);
    for (;;) {
        const uint32_t now = milliseconds;
        if ((int32_t)(now - next_step) >= 0) {
            next_step = now + 50u;
            volts = millivolts(1u) * 115u / 15u;  // panel, mV
            amps = millivolts(2u);                // load, mA
            power = volts * amps / 1000u;         // mW
            if (power < last_power) {
                step = -step;                     // went downhill: turn back
            }
            last_power = power;
            const int32_t next = (int32_t)duty + step;
            duty = next < 0 ? 0u : next > 1000 ? 1000u : (uint32_t)next;
            set_duty(duty);
            GPIOC_ODR ^= 1u << 13u;
        }
        if ((int32_t)(now - next_report) >= 0) {
            next_report = now + 250u;
            put_string("V ");
            put_number(volts);
            put_string(" mV I ");
            put_number(amps);
            put_string(" mA P ");
            put_number(power);
            put_string(" mW D ");
            put_number(duty / 10u);
            put_string(" %\r\n");
        }
        __asm volatile("wfi");
    }
}
