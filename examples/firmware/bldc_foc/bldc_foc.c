// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// Field-oriented control (FOC) of a PMSM motor with Hall sensors, in fixed
// point and without libraries, for SimNodus and for a real Blue Pill (72 MHz
// from the 8 MHz crystal; no FPU). Every PWM period (10 kHz, TIM2 update):
//   1. the phase currents ia, ib from the bridge's current-sense outputs
//      (offset measured at start with the bridge off), ic = -(ia + ib);
//   2. the electrical angle from the Hall sectors, interpolated between edges;
//   3. Clarke and Park: id, iq in the rotor's frame (q along the back-EMF);
//   4. two PI controllers: id towards 0, iq towards the setpoint;
//   5. inverse Park and Clarke, the min-max offset (space-vector equivalent)
//      and the three duties.
//   PA0, PA1, PA2 (TIM2_CH1..3): PWM to IN1..IN3; PB0, PB1, PB10: EN1..EN3, high.
//   PB5, PB6, PB7: Hall A, B, C with the internal pull-ups (EXTI, both edges).
//   PA6, PA7 (ADC1 IN6, IN7): SO1, SO2, 1.65 V + 0.2 V/A.
//   PA5 (ADC1 IN5): potentiometer, the torque current setpoint 0..2 A.
//   PC13 (on-board LED, lit when low): toggles every 24 Hall edges.
//   USART1 (PA9 TX, 115200 baud): "RPM <n> IQ* <mA> IQ <mA> ID <mA>" every 500 ms.
// Units: currents in mA, voltages in mV, angles 65536 = 360 degrees, sines Q15.
// The bus is taken as 12 V. Gains for R = 0.5 ohm, L = 0.5 mH and a current
// loop of about 300 Hz: Kp = 1 mV/mA, Ki = 0.094 mV/mA per period.
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
#define PERIOD 7200u        // 72 MHz / 10 kHz
#define BUS_MV 12000
#define LIMIT_MV 6900       // the largest phase voltage amplitude: bus / sqrt(3)
#define MV_PER_COUNT 806    // 3300 mV / 4095 counts, x 1000
#define MA_PER_MV 5         // 0.2 V/A: 5 mA per mV

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

// Electrical angle where each Hall code's sector starts (codes 000 and 111 cannot happen).
static const uint16_t sector_start[8] = {0, 60075, 38229, 49152, 16384, 5461, 27307, 0};

static volatile uint32_t milliseconds;
static volatile uint32_t edges;
static volatile uint16_t last_edge;     // TIM3 time of the last Hall edge, us
static volatile uint16_t sector_time;   // duration of the last sector, us
static volatile uint32_t code;
static volatile int32_t iq_target;      // mA
static volatile int32_t id_seen, iq_seen; // filtered, for the report
static int32_t offset_a, offset_b;      // ADC counts at zero current
static int32_t integral_d, integral_q;  // mV x 1000

void Default_Handler(void)
{
    for (;;) {
    }
}

void SysTick_Handler(void)
{
    ++milliseconds;
}

static int32_t sin_q15(uint32_t angle)
{
    return sine[(angle >> 8u) & 0xFFu];
}

static int32_t cos_q15(uint32_t angle)
{
    return sine[((angle + 16384u) >> 8u) & 0xFFu];
}

static uint32_t hall_code(void)
{
    const uint32_t idr = GPIOB_IDR;
    return ((idr >> 5u) & 1u) << 2u | ((idr >> 6u) & 1u) << 1u | ((idr >> 7u) & 1u);
}

static uint32_t convert(uint32_t channel)
{
    ADC1_SQR3 = channel;
    ADC1_CR2 = 1u;
    while (!(ADC1_SR & SR_EOC)) {
    }
    return ADC1_DR & 0xFFFu;
}

void EXTI9_5_IRQHandler(void)
{
    EXTI_PR = 7u << 5u;
    const uint16_t now = (uint16_t)TIM3_CNT;
    sector_time = (uint16_t)(now - last_edge);
    last_edge = now;
    code = hall_code();
    ++edges;
    if (edges % 24u == 0u) {
        GPIOC_ODR ^= 1u << 13u;
    }
}

static int32_t clamp(int32_t value, int32_t low, int32_t high)
{
    return value < low ? low : value > high ? high : value;
}

// One PI step: error in mA, output in mV; the integral is kept x 1000 and
// stops growing at the limit (anti-windup).
static int32_t pi(int32_t error, int32_t *integral)
{
    *integral = clamp(*integral + error * 94, -LIMIT_MV * 1000, LIMIT_MV * 1000);
    return clamp(error + *integral / 1000, -LIMIT_MV, LIMIT_MV);
}

void TIM2_IRQHandler(void)
{
    TIM2_SR = 0u;
    // 1. Phase currents (mA): the sense output's offset removed.
    const int32_t ia = ((int32_t)convert(6u) - offset_a) * MV_PER_COUNT / 1000 * MA_PER_MV;
    const int32_t ib = ((int32_t)convert(7u) - offset_b) * MV_PER_COUNT / 1000 * MA_PER_MV;
    // 2. Electrical angle: the sector's start plus the time since its edge.
    const uint32_t c = code;
    if (c == 0u || c == 7u) {
        return;
    }
    const uint16_t elapsed = (uint16_t)((uint16_t)TIM3_CNT - last_edge);
    const uint32_t span = sector_time;
    uint32_t within = 5461u; // half a sector while stopped or slow
    if (span > 0u && span < 50000u) {
        within = elapsed >= span ? 10922u : (uint32_t)elapsed * 10922u / span;
    }
    const uint32_t theta = (uint32_t)sector_start[c] + within;
    // The back-EMF of U is sin(theta): its vector points at theta - 90 degrees,
    // which is the q axis; the d axis is at theta - 180 degrees.
    const uint32_t phi = (theta - 32768u) & 0xFFFFu;
    const int32_t s = sin_q15(phi);
    const int32_t k = cos_q15(phi);
    // 3. Clarke (alpha, beta) and Park (d, q); 18919 = 32768 / sqrt(3).
    const int32_t alpha = ia;
    const int32_t beta = (ia + 2 * ib) * 18919 / 32768;
    const int32_t id = (alpha * k + beta * s) / 32768;
    const int32_t iq = (beta * k - alpha * s) / 32768;
    id_seen += (id - id_seen) / 64;
    iq_seen += (iq - iq_seen) / 64;
    // 4. PI controllers.
    const int32_t vd = pi(0 - id, &integral_d);
    const int32_t vq = pi(iq_target - iq, &integral_q);
    // 5. Inverse Park and Clarke; 28378 = 32768 x sqrt(3) / 2.
    const int32_t valpha = (vd * k - vq * s) / 32768;
    const int32_t vbeta = (vd * s + vq * k) / 32768;
    int32_t v[3];
    v[0] = valpha;
    v[1] = -valpha / 2 + vbeta * 28378 / 32768;
    v[2] = -valpha / 2 - vbeta * 28378 / 32768;
    const int32_t high = v[0] > v[1] ? (v[0] > v[2] ? v[0] : v[2]) : (v[1] > v[2] ? v[1] : v[2]);
    const int32_t low = v[0] < v[1] ? (v[0] < v[2] ? v[0] : v[2]) : (v[1] < v[2] ? v[1] : v[2]);
    const int32_t middle = (high + low) / 2;
    uint32_t d[3];
    for (int n = 0; n < 3; ++n) {
        const int32_t mv = BUS_MV / 2 + v[n] - middle;
        d[n] = (uint32_t)clamp(mv * (int32_t)PERIOD / BUS_MV, 0, (int32_t)PERIOD);
    }
    TIM2_CCR1 = d[0];
    TIM2_CCR2 = d[1];
    TIM2_CCR3 = d[2];
}

static void put_char(char ch)
{
    while (!(USART1_SR & SR_TXE)) {
    }
    USART1_DR = (uint8_t)ch;
}

static void put_string(const char *text)
{
    while (*text) {
        put_char(*text++);
    }
}

static void put_signed(int32_t value)
{
    char digits[11];
    int count = 0;
    uint32_t magnitude = value < 0 ? (uint32_t)(-value) : (uint32_t)value;
    do {
        digits[count++] = (char)('0' + magnitude % 10u);
        magnitude /= 10u;
    } while (magnitude && count < 10);
    if (value < 0) {
        put_char('-');
    }
    while (count) {
        put_char(digits[--count]);
    }
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

    // PA0-PA2: alternate-function push-pull 50 MHz; PA5-PA7: analog; PA9: USART1 TX.
    GPIOA_CRL = (GPIOA_CRL & ~0xFFF00FFFu) | 0xBBBu;
    GPIOA_CRH = (GPIOA_CRH & ~0xF0u) | (0xBu << 4u);
    // PB0, PB1: push-pull outputs; PB5-PB7: inputs with pull-up; PB10: push-pull output.
    GPIOB_CRL = (GPIOB_CRL & ~0xFFF000FFu) | 0x88800022u;
    GPIOB_CRH = (GPIOB_CRH & ~0xF00u) | 0x200u;
    GPIOB_ODR = (1u << 5u) | (1u << 6u) | (1u << 7u);
    GPIOC_CRH = (GPIOC_CRH & ~(0xFu << 20u)) | (0x2u << 20u);

    USART1_BRR = 625u;                      // 72 MHz / 115200
    USART1_CR1 = (1u << 13u) | (1u << 3u);  // UE, TE

    // TIM3: free-running microseconds for the Hall timing.
    TIM3_PSC = 71u;
    TIM3_ARR = 0xFFFFu;
    TIM3_EGR = 1u;
    TIM3_CR1 = 1u;

    // ADC1: power on, calibrate; 28.5-cycle sample time on IN5-IN7 (2.3 us each).
    ADC1_CR2 = 1u;
    for (volatile int i = 0; i < 100; ++i) {
    }
    ADC1_CR2 = 1u | (1u << 2u);
    while (ADC1_CR2 & (1u << 2u)) {
    }
    ADC1_SMPR2 = (3u << 15u) | (3u << 18u) | (3u << 21u);

    SYST_RVR = 72000u - 1u;                 // 1 ms
    SYST_CVR = 0u;
    SYST_CSR = 7u;

    // The sense outputs' zero, with the bridge still off (EN low).
    for (volatile int i = 0; i < 20000; ++i) {
    }
    int32_t sum_a = 0, sum_b = 0;
    for (int n = 0; n < 16; ++n) {
        sum_a += (int32_t)convert(6u);
        sum_b += (int32_t)convert(7u);
    }
    offset_a = sum_a / 16;
    offset_b = sum_b / 16;

    // Hall inputs PB5-PB7 on EXTI5-EXTI7, both edges.
    AFIO_EXTICR2 = (1u << 4u) | (1u << 8u) | (1u << 12u);
    EXTI_RTSR = 7u << 5u;
    EXTI_FTSR = 7u << 5u;
    EXTI_IMR = 7u << 5u;
    code = hall_code();
    last_edge = (uint16_t)TIM3_CNT;
    NVIC_ISER0 = 1u << 23u;

    // TIM2: edge-aligned PWM mode 1 with preload on CH1..CH3, update interrupt.
    TIM2_PSC = 0u;
    TIM2_ARR = PERIOD - 1u;
    TIM2_CCMR1 = (6u << 4u) | (1u << 3u) | (6u << 12u) | (1u << 11u);
    TIM2_CCMR2 = (6u << 4u) | (1u << 3u);
    TIM2_CCER = 1u | (1u << 4u) | (1u << 8u);
    TIM2_CCR1 = PERIOD / 2u;
    TIM2_CCR2 = PERIOD / 2u;
    TIM2_CCR3 = PERIOD / 2u;
    TIM2_EGR = 1u;
    TIM2_DIER = 1u;
    GPIOB_BSRR = (1u << 0u) | (1u << 1u) | (1u << 10u); // EN1-EN3 high
    TIM2_CR1 = (1u << 7u) | 1u;
    NVIC_ISER0 = 1u << 28u;

    put_string("PMSM field-oriented control: the potentiometer sets the torque current\r\n");
    uint32_t next_set = 0, next_report = 500u, counted = 0;
    for (;;) {
        const uint32_t now = milliseconds;
        if ((int32_t)(now - next_set) >= 0) {
            next_set = now + 10u;
            __asm volatile("cpsid i");
            const uint32_t pot = convert(5u);
            code = hall_code(); // also from rest, when no edge comes
            __asm volatile("cpsie i");
            iq_target = (int32_t)(pot * 2000u / 4095u);
        }
        if ((int32_t)(now - next_report) >= 0) {
            next_report = now + 500u;
            const uint32_t total = edges;
            const uint32_t rpm = (total - counted) * 5u; // 24 edges per turn, twice a second
            counted = total;
            put_string("RPM ");
            put_signed((int32_t)rpm);
            put_string(" IQ* ");
            put_signed(iq_target);
            put_string(" IQ ");
            put_signed(iq_seen);
            put_string(" ID ");
            put_signed(id_seen);
            put_string("\r\n");
        }
        __asm volatile("wfi");
    }
}
