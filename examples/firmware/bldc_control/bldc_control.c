// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// A BLDC motor driven from its Hall sensors, without libraries, for SimNodus
// and for a real Blue Pill (72 MHz from the 8 MHz crystal). Two modes:
//   six-step (default): in each 60-degree sector one phase gets the PWM, one
//   is held low and the third is left open (EN low);
//   sinusoidal (PB12 to GND): all three phases get sine-weighted PWM with the
//   min-max (space-vector equivalent) offset, at the electrical angle
//   estimated between Hall edges; the first step towards FOC.
//   PA0, PA1, PA2 (TIM2_CH1..3): PWM to the bridge's IN1..IN3, 10 kHz
//   (PB13 to GND: 5 kHz; PB14 to GND: 20 kHz).
//   PB0, PB1, PB10: EN1..EN3.
//   PB5, PB6, PB7: Hall A, B, C with the internal pull-ups (EXTI, both edges).
//   PA4 (ADC1 IN4): potentiometer, the duty (six-step) or amplitude (sine).
//   PC13 (on-board LED, lit when low): toggles every 24 Hall edges.
//   USART1 (PA9 TX, 115200 baud): "RPM <n> DUTY <percent> % HALL <ABC>" every 500 ms
//   (4 pole pairs: 24 Hall edges per turn).
// Hall codes (A B C) and the phases driven for forward rotation:
//   101 U+ V-, 100 U+ W-, 110 V+ W-, 010 V+ U-, 011 W+ U-, 001 W+ V-.
#include <stdint.h>

#define REG(address) (*(volatile uint32_t *)(address))
#define RCC_CR REG(0x40021000u)
#define RCC_CFGR REG(0x40021004u)
#define RCC_APB2ENR REG(0x40021018u)
#define RCC_APB1ENR REG(0x4002101Cu)
#define FLASH_ACR REG(0x40022000u)
#define GPIOA_CRL REG(0x40010800u)
#define GPIOA_CRH REG(0x40010804u)
#define GPIOB_CRL REG(0x40010C00u)
#define GPIOB_CRH REG(0x40010C04u)
#define GPIOB_IDR REG(0x40010C08u)
#define GPIOB_ODR REG(0x40010C0Cu)
#define GPIOB_BSRR REG(0x40010C10u)
#define GPIOC_CRH REG(0x40011004u)
#define GPIOC_ODR REG(0x4001100Cu)
#define AFIO_EXTICR2 REG(0x4001000Cu)
#define EXTI_IMR REG(0x40010400u)
#define EXTI_RTSR REG(0x40010408u)
#define EXTI_FTSR REG(0x4001040Cu)
#define EXTI_PR REG(0x40010414u)
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
#define TIM2_DIER REG(0x4000000Cu)
#define TIM2_SR REG(0x40000010u)
#define TIM2_EGR REG(0x40000014u)
#define TIM2_CCMR1 REG(0x40000018u)
#define TIM2_CCMR2 REG(0x4000001Cu)
#define TIM2_CCER REG(0x40000020u)
#define TIM2_PSC REG(0x40000028u)
#define TIM2_ARR REG(0x4000002Cu)
#define TIM2_CCR1 REG(0x40000034u)
#define TIM2_CCR2 REG(0x40000038u)
#define TIM2_CCR3 REG(0x4000003Cu)
#define TIM3_CR1 REG(0x40000400u)
#define TIM3_EGR REG(0x40000414u)
#define TIM3_CNT REG(0x40000424u)
#define TIM3_PSC REG(0x40000428u)
#define TIM3_ARR REG(0x4000042Cu)
#define SYST_CSR REG(0xE000E010u)
#define SYST_RVR REG(0xE000E014u)
#define SYST_CVR REG(0xE000E018u)
#define NVIC_ISER0 REG(0xE000E100u)
#define SCB_VTOR REG(0xE000ED08u)

#define SR_TXE (1u << 7u)
#define SR_EOC (1u << 1u)

extern uint32_t _data_load, _data_start, _data_end, _bss_start, _bss_end, _stack_top;

void Reset_Handler(void);
void Default_Handler(void);
void SysTick_Handler(void);
void EXTI9_5_IRQHandler(void);
void TIM2_IRQHandler(void);

// Core exceptions and IRQ0..IRQ28 (EXTI9_5 is IRQ23, TIM2 is IRQ28).
__attribute__((section(".vectors"), used))
const uintptr_t vectors[16 + 29] = {
    [0] = (uintptr_t)&_stack_top,
    [1] = (uintptr_t)Reset_Handler,
    [2 ... 6] = (uintptr_t)Default_Handler,
    [11] = (uintptr_t)Default_Handler,
    [12] = (uintptr_t)Default_Handler,
    [14] = (uintptr_t)Default_Handler,
    [15] = (uintptr_t)SysTick_Handler,
    [16 ... 38] = (uintptr_t)Default_Handler,
    [16 + 23] = (uintptr_t)EXTI9_5_IRQHandler,
    [40 ... 43] = (uintptr_t)Default_Handler,
    [16 + 28] = (uintptr_t)TIM2_IRQHandler,
};

static const int16_t sine[256] = {
    0, 804, 1608, 2410, 3212, 4011, 4808, 5602, 6393, 7179, 7962, 8739, 9512, 10278, 11039, 11793,
    12539, 13279, 14010, 14732, 15446, 16151, 16846, 17530, 18204, 18868, 19519, 20159, 20787, 21403, 22005, 22594,
    23170, 23731, 24279, 24811, 25329, 25832, 26319, 26790, 27245, 27683, 28105, 28510, 28898, 29268, 29621, 29956,
    30273, 30571, 30852, 31113, 31356, 31580, 31785, 31971, 32137, 32285, 32412, 32521, 32609, 32678, 32728, 32757,
    32767, 32757, 32728, 32678, 32609, 32521, 32412, 32285, 32137, 31971, 31785, 31580, 31356, 31113, 30852, 30571,
    30273, 29956, 29621, 29268, 28898, 28510, 28105, 27683, 27245, 26790, 26319, 25832, 25329, 24811, 24279, 23731,
    23170, 22594, 22005, 21403, 20787, 20159, 19519, 18868, 18204, 17530, 16846, 16151, 15446, 14732, 14010, 13279,
    12539, 11793, 11039, 10278, 9512, 8739, 7962, 7179, 6393, 5602, 4808, 4011, 3212, 2410, 1608, 804,
    0, -804, -1608, -2410, -3212, -4011, -4808, -5602, -6393, -7179, -7962, -8739, -9512, -10278, -11039, -11793,
    -12539, -13279, -14010, -14732, -15446, -16151, -16846, -17530, -18204, -18868, -19519, -20159, -20787, -21403, -22005, -22594,
    -23170, -23731, -24279, -24811, -25329, -25832, -26319, -26790, -27245, -27683, -28105, -28510, -28898, -29268, -29621, -29956,
    -30273, -30571, -30852, -31113, -31356, -31580, -31785, -31971, -32137, -32285, -32412, -32521, -32609, -32678, -32728, -32757,
    -32767, -32757, -32728, -32678, -32609, -32521, -32412, -32285, -32137, -31971, -31785, -31580, -31356, -31113, -30852, -30571,
    -30273, -29956, -29621, -29268, -28898, -28510, -28105, -27683, -27245, -26790, -26319, -25832, -25329, -24811, -24279, -23731,
    -23170, -22594, -22005, -21403, -20787, -20159, -19519, -18868, -18204, -17530, -16846, -16151, -15446, -14732, -14010, -13279,
    -12539, -11793, -11039, -10278, -9512, -8739, -7962, -7179, -6393, -5602, -4808, -4011, -3212, -2410, -1608, -804,
};

// Phases per Hall code (A << 2 | B << 1 | C): the one with the PWM and the
// one held low (0 = U, 1 = V, 2 = W); -1 for the impossible codes 000, 111.
static const int8_t high_phase[8] = {-1, 2, 1, 2, 0, 0, 1, -1};
static const int8_t low_phase[8] = {-1, 1, 0, 0, 2, 1, 2, -1};
// Electrical angle where each code's sector starts, 65536 = 360 degrees.
static const uint16_t sector_start[8] = {0, 60075, 38229, 49152, 16384, 5461, 27307, 0};

static volatile uint32_t milliseconds;
static volatile uint32_t edges;
static volatile uint32_t level;         // duty or amplitude, 0..4095
static volatile uint16_t last_edge;     // TIM3 time of the last Hall edge, us
static volatile uint16_t sector_time;   // duration of the last sector, us
static volatile uint32_t code;
static uint32_t period;                 // TIM2 counts per PWM period
static int sinusoidal;

void Default_Handler(void)
{
    for (;;) {
    }
}

void SysTick_Handler(void)
{
    ++milliseconds;
}

static uint32_t hall_code(void)
{
    const uint32_t idr = GPIOB_IDR;
    return ((idr >> 5u) & 1u) << 2u | ((idr >> 6u) & 1u) << 1u | ((idr >> 7u) & 1u);
}

static void set_enables(uint32_t mask)
{
    static const uint32_t pins[3] = {0u, 1u, 10u};
    uint32_t bsrr = 0;
    for (int k = 0; k < 3; ++k) {
        bsrr |= (mask & (1u << k)) ? (1u << pins[k]) : (1u << (pins[k] + 16u));
    }
    GPIOB_BSRR = bsrr;
}

static void set_duties(uint32_t u, uint32_t v, uint32_t w)
{
    TIM2_CCR1 = u;
    TIM2_CCR2 = v;
    TIM2_CCR3 = w;
}

// Six-step: the phase with the PWM, the low one and the open one.
static void commutate(uint32_t c)
{
    const int high = high_phase[c];
    const int low = low_phase[c];
    if (high < 0) {
        set_enables(0u);
        return;
    }
    const uint32_t duty = level * period / 4095u;
    uint32_t d[3] = {0u, 0u, 0u};
    d[high] = duty;
    set_duties(d[0], d[1], d[2]);
    set_enables((1u << high) | (1u << low));
}

void EXTI9_5_IRQHandler(void)
{
    EXTI_PR = 7u << 5u;
    const uint16_t now = (uint16_t)TIM3_CNT;
    sector_time = (uint16_t)(now - last_edge);
    last_edge = now;
    code = hall_code();
    ++edges;
    if (!sinusoidal) {
        commutate(code);
    }
    if (edges % 24u == 0u) {
        GPIOC_ODR ^= 1u << 13u;
    }
}

// Sinusoidal: every PWM period, the angle between Hall edges and three duties.
void TIM2_IRQHandler(void)
{
    TIM2_SR = 0u;
    const uint32_t c = code;
    if (high_phase[c] < 0) {
        return;
    }
    const uint16_t elapsed = (uint16_t)((uint16_t)TIM3_CNT - last_edge);
    const uint32_t span = sector_time;
    uint32_t within = 5461u; // half a sector while stopped or slow
    if (span > 0u && span < 50000u) {
        within = elapsed >= span ? 10922u : (uint32_t)elapsed * 10922u / span;
    }
    // The phase voltages follow the back-EMF: e_U = sin(angle), so v_U too.
    const uint32_t angle = (uint32_t)sector_start[c] + within;
    int32_t s[3];
    for (int k = 0; k < 3; ++k) {
        const uint32_t a = (angle - (uint32_t)k * 21845u) & 0xFFFFu;
        s[k] = sine[a >> 8u];
    }
    const int32_t high = s[0] > s[1] ? (s[0] > s[2] ? s[0] : s[2]) : (s[1] > s[2] ? s[1] : s[2]);
    const int32_t low = s[0] < s[1] ? (s[0] < s[2] ? s[0] : s[2]) : (s[1] < s[2] ? s[1] : s[2]);
    const int32_t offset = (high + low) / 2;
    uint32_t d[3];
    for (int k = 0; k < 3; ++k) {
        // (s - offset) spans +-sqrt(3)/2 of 32767 = 28377: half a period at full level.
        const int32_t centered = (s[k] - offset) * (int32_t)level / 4095;
        d[k] = (uint32_t)((int32_t)period / 2 + centered * ((int32_t)period / 2) / 28377);
    }
    set_duties(d[0], d[1], d[2]);
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

static uint32_t convert(uint32_t channel)
{
    ADC1_SQR3 = channel;
    ADC1_CR2 = 1u;
    while (!(ADC1_SR & SR_EOC)) {
    }
    return ADC1_DR & 0xFFFu;
}

static void clock_72mhz(void)
{
    RCC_CR |= 1u << 16u;                    // HSEON
    while (!(RCC_CR & (1u << 17u))) {
    }
    FLASH_ACR = 0x12u;                      // prefetch, two wait states
    // PLL from HSE x 9, APB1 / 2, ADC / 6.
    RCC_CFGR = (1u << 16u) | (7u << 18u) | (4u << 8u) | (2u << 14u);
    RCC_CR |= 1u << 24u;                    // PLLON
    while (!(RCC_CR & (1u << 25u))) {
    }
    RCC_CFGR |= 2u;                         // SYSCLK = PLL
    while ((RCC_CFGR & 0xCu) != 0x8u) {
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
    clock_72mhz();
    // AFIOEN, IOPAEN, IOPBEN, IOPCEN, ADC1EN, USART1EN; TIM2EN, TIM3EN.
    RCC_APB2ENR |= (1u << 0u) | (1u << 2u) | (1u << 3u) | (1u << 4u) | (1u << 9u) | (1u << 14u);
    RCC_APB1ENR |= (1u << 0u) | (1u << 1u);

    // PA0-PA2: alternate-function push-pull 50 MHz; PA4: analog; PA9: USART1 TX.
    GPIOA_CRL = (GPIOA_CRL & ~0xF0FFFu) | 0xBBBu;
    GPIOA_CRH = (GPIOA_CRH & ~0xF0u) | (0xBu << 4u);
    // PB0, PB1: push-pull outputs; PB5-PB7: inputs with pull-up.
    GPIOB_CRL = (GPIOB_CRL & ~0xFFF000FFu) | 0x88800022u;
    // PB10: push-pull output; PB12-PB14: inputs with pull-up.
    GPIOB_CRH = (GPIOB_CRH & ~0x0FFF0F00u) | 0x08880200u;
    GPIOB_ODR = (1u << 5u) | (1u << 6u) | (1u << 7u) | (1u << 12u) | (1u << 13u) | (1u << 14u);
    GPIOC_CRH = (GPIOC_CRH & ~(0xFu << 20u)) | (0x2u << 20u);
    set_enables(0u);

    USART1_BRR = 625u;                      // 72 MHz / 115200
    USART1_CR1 = (1u << 13u) | (1u << 3u);  // UE, TE

    for (volatile int i = 0; i < 1000; ++i) { // let the pull-ups settle
    }
    sinusoidal = (GPIOB_IDR & (1u << 12u)) == 0u;
    const uint32_t khz = (GPIOB_IDR & (1u << 13u)) == 0u ? 5u : (GPIOB_IDR & (1u << 14u)) == 0u ? 20u : 10u;
    period = 72000u / khz;

    // TIM3: free-running microseconds for the Hall timing.
    TIM3_PSC = 71u;
    TIM3_ARR = 0xFFFFu;
    TIM3_EGR = 1u;
    TIM3_CR1 = 1u;

    // TIM2: edge-aligned PWM mode 1 with preload on CH1..CH3.
    TIM2_PSC = 0u;
    TIM2_ARR = period - 1u;
    TIM2_CCMR1 = (6u << 4u) | (1u << 3u) | (6u << 12u) | (1u << 11u);
    TIM2_CCMR2 = (6u << 4u) | (1u << 3u);
    TIM2_CCER = 1u | (1u << 4u) | (1u << 8u);
    set_duties(0u, 0u, 0u);
    TIM2_EGR = 1u;
    TIM2_CR1 = (1u << 7u) | 1u;

    // ADC1: power on, calibrate, 239.5-cycle sample time on IN4.
    ADC1_CR2 = 1u;
    for (volatile int i = 0; i < 100; ++i) {
    }
    ADC1_CR2 = 1u | (1u << 2u);
    while (ADC1_CR2 & (1u << 2u)) {
    }
    ADC1_SMPR2 = 7u << 12u;
    level = convert(4u);

    // Hall inputs PB5-PB7 on EXTI5-EXTI7, both edges.
    AFIO_EXTICR2 = (1u << 4u) | (1u << 8u) | (1u << 12u);
    EXTI_RTSR = 7u << 5u;
    EXTI_FTSR = 7u << 5u;
    EXTI_IMR = 7u << 5u;
    code = hall_code();
    last_edge = (uint16_t)TIM3_CNT;
    NVIC_ISER0 = 1u << 23u;
    if (sinusoidal) {
        set_enables(7u);
        TIM2_DIER = 1u;                     // update interrupt
        NVIC_ISER0 = 1u << 28u;
    } else {
        commutate(code);
    }

    SYST_RVR = 72000u - 1u;                 // 1 ms
    SYST_CVR = 0u;
    SYST_CSR = 7u;

    put_string(sinusoidal ? "BLDC sinusoidal, PWM " : "BLDC six-step, PWM ");
    put_number(khz);
    put_string(" kHz: turn the potentiometer\r\n");
    uint32_t next_adc = 0, next_report = 500u, counted = 0;
    for (;;) {
        const uint32_t now = milliseconds;
        if ((int32_t)(now - next_adc) >= 0) {
            next_adc = now + 10u;
            level = convert(4u);
            // The Hall code read again: starts the motor from rest, when no edge comes.
            __asm volatile("cpsid i");
            code = hall_code();
            if (!sinusoidal) {
                commutate(code); // and the new duty
            }
            __asm volatile("cpsie i");
        }
        if ((int32_t)(now - next_report) >= 0) {
            next_report = now + 500u;
            const uint32_t total = edges;
            const uint32_t rpm = (total - counted) * 5u; // 24 edges per turn, twice a second
            counted = total;
            put_string("RPM ");
            put_number(rpm);
            put_string(" DUTY ");
            put_number(level * 100u / 4095u);
            const uint32_t c = code;
            put_string(" % HALL ");
            put_char((char)('0' + ((c >> 2u) & 1u)));
            put_char((char)('0' + ((c >> 1u) & 1u)));
            put_char((char)('0' + (c & 1u)));
            put_string("\r\n");
        }
        __asm volatile("wfi");
    }
}
