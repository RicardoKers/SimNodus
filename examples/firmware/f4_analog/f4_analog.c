// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// STM32F407 timers, ADC and DAC (experiment E-12, SN-297), at 16 MHz from the HSI.
//   PA1: ADC1 channel 1, a voltage from the circuit (a potentiometer),
//        converted every millisecond on TIM2's update event (TRGO).
//   PA8 / PB13: TIM1 channel 1 and its complementary output, 20 kHz PWM
//        with 0.5 us of dead time; the duty cycle follows PA1.
//   PA4: DAC channel 1 gives PA1's code back (the same voltage).
//   PA5: DAC channel 2, a triangle of 1023 codes above 1536 (1.24 V to 2.06 V),
//        one step on each update of TIM6 (every 100 us): about 4.9 Hz.
//   PA9: USART1 at 115200 baud prints the code every 100 ms: "adc 1241".
#define F4_ADC ADC_IRQHandler
#include "../f4_common/f4.h"

#define TIM1_BASE 0x40010000u
#define TIM2_BASE 0x40000000u
#define TIM6_BASE 0x40001000u
#define TIM_CR1(base) REG((base) + 0x00u)
#define TIM_CR2(base) REG((base) + 0x04u)
#define TIM_EGR(base) REG((base) + 0x14u)
#define TIM_CCMR1(base) REG((base) + 0x18u)
#define TIM_CCER(base) REG((base) + 0x20u)
#define TIM_PSC(base) REG((base) + 0x28u)
#define TIM_ARR(base) REG((base) + 0x2Cu)
#define TIM_CCR1(base) REG((base) + 0x34u)
#define TIM_BDTR(base) REG((base) + 0x44u)

#define ADC1_SR REG(0x40012000u)
#define ADC1_CR1 REG(0x40012004u)
#define ADC1_CR2 REG(0x40012008u)
#define ADC1_SMPR2 REG(0x40012010u)
#define ADC1_SQR3 REG(0x40012034u)
#define ADC1_DR REG(0x4001204Cu)

#define DAC_CR REG(0x40007400u)
#define DAC_DHR12R1 REG(0x40007408u)
#define DAC_DHR12R2 REG(0x40007414u)

enum { PWM_PERIOD = 800 }; // 16 MHz / 800 = 20 kHz

static volatile uint32_t code;
static volatile uint32_t conversions;

void ADC_IRQHandler(void)
{
    const uint32_t value = ADC1_DR; // reading DR clears EOC
    code = value;
    TIM_CCR1(TIM1_BASE) = value * PWM_PERIOD / 4096u;
    DAC_DHR12R1 = value;
    ++conversions;
}

int main(void)
{
    RCC_AHB1ENR |= (1u << 0) | (1u << 1);              // GPIOAEN, GPIOBEN
    RCC_APB1ENR |= (1u << 0) | (1u << 4) | (1u << 29); // TIM2EN, TIM6EN, DACEN
    RCC_APB2ENR |= (1u << 0) | (1u << 8);              // TIM1EN, ADC1EN
    usart1_start(139);                                 // 16 MHz / 115200

    // PA1, PA4, PA5 analog; PA8 TIM1_CH1 and PB13 TIM1_CH1N (alternate function 1).
    GPIO_MODER(PORT_A) = (GPIO_MODER(PORT_A) & ~((3u << 2) | (3u << 8) | (3u << 10) | (3u << 16)))
        | (3u << 2) | (3u << 8) | (3u << 10) | (2u << 16);
    GPIO_AFRH(PORT_A) = (GPIO_AFRH(PORT_A) & ~15u) | 1u;
    GPIO_MODER(PORT_B) = (GPIO_MODER(PORT_B) & ~(3u << 26)) | (2u << 26);
    GPIO_AFRH(PORT_B) = (GPIO_AFRH(PORT_B) & ~(15u << 20)) | (1u << 20);

    // TIM1: PWM mode 1 with preload, both outputs, 8 clock periods of dead time.
    TIM_ARR(TIM1_BASE) = PWM_PERIOD - 1u;
    TIM_CCR1(TIM1_BASE) = 0;
    TIM_CCMR1(TIM1_BASE) = (6u << 4) | (1u << 3); // OC1M = PWM 1, OC1PE
    TIM_CCER(TIM1_BASE) = (1u << 0) | (1u << 2);  // CC1E, CC1NE
    TIM_BDTR(TIM1_BASE) = (1u << 15) | 8u;        // MOE, DTG = 8
    TIM_EGR(TIM1_BASE) = 1u;                      // UG
    TIM_CR1(TIM1_BASE) = (1u << 7) | 1u;          // ARPE, CEN

    // The DAC: channel 1 without a trigger, channel 2 a triangle on TIM6's TRGO.
    DAC_DHR12R1 = 0;
    DAC_DHR12R2 = 1536;
    DAC_CR = (1u << 0)                                          // EN1
        | (1u << 16) | (1u << 18) | (0u << 19) | (2u << 22) | (9u << 24); // EN2, TEN2, TIM6, triangle, 1023
    TIM_ARR(TIM6_BASE) = 1599;  // 100 us
    TIM_CR2(TIM6_BASE) = 2u << 4; // MMS: update
    TIM_CR1(TIM6_BASE) = 1u;

    // ADC1: channel 1 with 84 cycles of sampling, started by TIM2's TRGO, an interrupt at the end.
    ADC1_SMPR2 = 4u << 3;
    ADC1_SQR3 = 1u;
    ADC1_CR1 = 1u << 5;                                  // EOCIE
    ADC1_CR2 = (1u << 0) | (1u << 28) | (6u << 24);       // ADON, EXTEN rising, EXTSEL = TIM2 TRGO
    NVIC_ISER0 = 1u << 18;                               // ADC
    TIM_ARR(TIM2_BASE) = 15999;  // 1 ms
    TIM_CR2(TIM2_BASE) = 2u << 4; // MMS: update
    TIM_CR1(TIM2_BASE) = 1u;

    put_text("f4_analog\r\n");
    uint32_t printed = 0;
    for (;;) {
        __asm__ volatile("wfi");
        if (conversions - printed >= 100u) {
            printed = conversions;
            put_text("adc ");
            put_decimal(code);
            put_text("\r\n");
        }
    }
}
