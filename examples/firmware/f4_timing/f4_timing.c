// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// Cycle measurements for the STM32F407's timing model (experiment E-12,
// SN-296), for a real chip and for SimNodus, as cycle_loops is for the Blue
// Pill: each test runs a loop and counts its core cycles with DWT CYCCNT.
//
// The tests run four times:
//   1. HSI 16 MHz, no flash wait state, the ART accelerator off (reset state);
//   2. 168 MHz from the 25 MHz crystal, 5 wait states, prefetch and both
//      caches on (as STM32CubeMX sets it up);
//   3. the same with the caches on and prefetch off;
//   4. the same with prefetch and both caches off.
// The results are printed on USART1 (PA9, 115200 baud) every few seconds as
// "name cycles" lines, so a serial terminal can join at any time. The
// firmware needs the 25 MHz crystal (the owner's board, example 67).
#define F4_SYSTICK SysTick_Handler
#include "../f4_common/f4.h"

#define HSE_VALUE 25000000u
#define SCB_ICSR REG(0xE000ED04u)
#define ACR_PRFTEN (1u << 8)
#define ACR_ICEN (1u << 9)
#define ACR_DCEN (1u << 10)

volatile uint32_t scratch[8] = {1, 2, 3, 4, 5, 6, 7, 8};
__attribute__((used)) const uint32_t flash_word = 0x12345678u;
static volatile uint32_t irq_count;
static volatile uint32_t irq_uses_fpu;
static volatile float irq_float = 1.0f;

// Pended by the irq tests through ICSR.PENDSTSET; the timer itself stays off.
void SysTick_Handler(void)
{
    ++irq_count;
    if (irq_uses_fpu) {
        irq_float = irq_float * 1.5f;
    }
}

// Each loop takes the iteration count in r0 and returns the cycles it took.
// r3 points at CYCCNT, r6 at `scratch`, r7 at `flash_word`; the body is the
// instructions under test, then "subs r0, #1; bne 1b". Loops start on an
// 8-byte boundary; `long` is 2 KiB, twice the ART accelerator's instruction
// cache.
#define LOOP(name, setup, body)                                                                                      \
    ".balign 16\n.global " #name "\n.type " #name ", %function\n.thumb_func\n" #name ":\n"                         \
    "push {r4-r7, lr}\nldr r3, =0xE0001004\nldr r6, =scratch\nldr r7, =flash_word\n" setup "\nldr r4, [r3]\n"      \
    ".balign 8\n1:\n" body "\nsubs r0, #1\nbne 1b\nldr r5, [r3]\nsubs r0, r5, r4\npop {r4-r7, pc}\n.ltorg\n"
#define REPEAT(count, instruction) ".rept " #count "\n" instruction "\n.endr"

__asm__(".syntax unified\n.thumb\n.fpu fpv4-sp-d16\n.section .text.loops,\"ax\",%progbits\n"
        ".balign 4\n.thumb_func\nreturn_only:\nbx lr\n"
        LOOP(loop_empty, "", "")
        LOOP(loop_alu, "movs r1, #0\nmovs r2, #3", REPEAT(8, "adds r1, r1, r2"))
        LOOP(loop_ldr, "", REPEAT(8, "ldr r1, [r6]"))
        LOOP(loop_ldr_alu, "movs r2, #0", REPEAT(4, "ldr r1, [r6]\nadds r2, r2, r1"))
        LOOP(loop_ldr_flash, "", REPEAT(8, "ldr r1, [r7]"))
        LOOP(loop_str, "movs r1, #5", REPEAT(8, "str r1, [r6]"))
        LOOP(loop_mul, "movs r1, #3\nmovs r2, #5", REPEAT(8, "muls r1, r2, r1"))
        LOOP(loop_udiv_small, "movs r2, #100\nmovs r5, #7", REPEAT(8, "udiv r1, r2, r5"))
        LOOP(loop_udiv_large, "movs r2, #0\nsubs r2, #1\nmovs r5, #3", REPEAT(8, "udiv r1, r2, r5"))
        LOOP(loop_branch, "", REPEAT(8, "b 2f\n2:"))
        LOOP(loop_call, "", REPEAT(8, "bl return_only"))
        LOOP(loop_ldm, "", REPEAT(8, "ldmia r6, {r1, r2, r5, r7}"))
        LOOP(loop_push_pop, "", REPEAT(4, "push {r1, r2, r4, r5}\npop {r1, r2, r4, r5}"))
        LOOP(loop_smlad, "movs r1, #3\nmovs r2, #5", REPEAT(8, "smlad r1, r1, r2, r1"))
        LOOP(loop_vadd, "vmov.f32 s0, #1.0\nvmov.f32 s1, #0.5", REPEAT(8, "vadd.f32 s0, s0, s1"))
        LOOP(loop_vmul, "vmov.f32 s0, #1.0\nvmov.f32 s1, #0.5", REPEAT(8, "vmul.f32 s0, s0, s1"))
        LOOP(loop_vfma, "vmov.f32 s0, #1.0\nvmov.f32 s1, #0.5\nvmov.f32 s2, #0.25", REPEAT(8, "vfma.f32 s0, s1, s2"))
        LOOP(loop_vdiv, "vmov.f32 s1, #3.0\nvmov.f32 s2, #0.5", REPEAT(8, "vdiv.f32 s0, s1, s2"))
        LOOP(loop_vsqrt, "vmov.f32 s1, #3.0", REPEAT(8, "vsqrt.f32 s0, s1"))
        LOOP(loop_vldr, "", REPEAT(8, "vldr s0, [r6]"))
        LOOP(loop_vmov, "vmov.f32 s0, #1.0", REPEAT(8, "vmov r1, s0"))
        LOOP(loop_long, "movs r1, #0\nmovs r2, #3", REPEAT(1024, "adds r1, r1, r2")));

uint32_t loop_empty(uint32_t n);
uint32_t loop_alu(uint32_t n);
uint32_t loop_ldr(uint32_t n);
uint32_t loop_ldr_alu(uint32_t n);
uint32_t loop_ldr_flash(uint32_t n);
uint32_t loop_str(uint32_t n);
uint32_t loop_mul(uint32_t n);
uint32_t loop_udiv_small(uint32_t n);
uint32_t loop_udiv_large(uint32_t n);
uint32_t loop_branch(uint32_t n);
uint32_t loop_call(uint32_t n);
uint32_t loop_ldm(uint32_t n);
uint32_t loop_push_pop(uint32_t n);
uint32_t loop_smlad(uint32_t n);
uint32_t loop_vadd(uint32_t n);
uint32_t loop_vmul(uint32_t n);
uint32_t loop_vfma(uint32_t n);
uint32_t loop_vdiv(uint32_t n);
uint32_t loop_vsqrt(uint32_t n);
uint32_t loop_vldr(uint32_t n);
uint32_t loop_vmov(uint32_t n);
uint32_t loop_long(uint32_t n);

// Plain C, as the compiler writes it: bubble sort, a bitwise CRC-32 and a
// floating-point low-pass filter.
static uint32_t c_sort(uint32_t n)
{
    static uint32_t data[32];
    const uint32_t start = DWT_CYCCNT;
    for (uint32_t round = 0; round < n / 100u; ++round) {
        uint32_t seed = 12345u + round;
        for (uint32_t i = 0; i < 32u; ++i) {
            seed = seed * 1103515245u + 12345u;
            data[i] = seed >> 16u;
        }
        for (uint32_t i = 0; i < 31u; ++i) {
            for (uint32_t j = 0; j < 31u - i; ++j) {
                if (data[j] > data[j + 1u]) {
                    const uint32_t t = data[j];
                    data[j] = data[j + 1u];
                    data[j + 1u] = t;
                }
            }
        }
    }
    return DWT_CYCCNT - start;
}

static uint32_t c_crc(uint32_t n)
{
    const uint32_t start = DWT_CYCCNT;
    uint32_t crc = 0xFFFFFFFFu;
    for (uint32_t i = 0; i < n; ++i) {
        crc ^= i & 0xFFu;
        for (int bit = 0; bit < 8; ++bit) {
            crc = (crc >> 1u) ^ (0xEDB88320u & (0u - (crc & 1u)));
        }
    }
    scratch[0] = crc;
    return DWT_CYCCNT - start;
}

static uint32_t c_filter(uint32_t n)
{
    const uint32_t start = DWT_CYCCNT;
    float y1 = 0.0f;
    float y2 = 0.0f;
    for (uint32_t i = 0; i < n; ++i) {
        const float x = (float)(i & 63u) * 0.015625f;
        const float y = 0.2f * x + 1.1f * y1 - 0.3f * y2;
        y2 = y1;
        y1 = y;
    }
    irq_float = y1;
    return DWT_CYCCNT - start;
}

// Exception entry and return: SysTick pended n / 10 times. In irq_fpu the
// handler uses the FPU, so lazy stacking saves the thread's registers.
static uint32_t c_irq(uint32_t n)
{
    irq_uses_fpu = 0;
    const uint32_t start = DWT_CYCCNT;
    for (uint32_t i = 0; i < n / 10u; ++i) {
        SCB_ICSR = 1u << 26; // PENDSTSET
    }
    return DWT_CYCCNT - start;
}

static uint32_t c_irq_fpu(uint32_t n)
{
    irq_uses_fpu = 1;
    const uint32_t start = DWT_CYCCNT;
    for (uint32_t i = 0; i < n / 10u; ++i) {
        SCB_ICSR = 1u << 26; // PENDSTSET
    }
    const uint32_t cycles = DWT_CYCCNT - start;
    irq_uses_fpu = 0;
    return cycles;
}

typedef uint32_t (*Test)(uint32_t);
#define ITERATIONS 1000u
static const struct {
    const char *name;
    Test run;
    uint32_t iterations;
} tests[] = {
    {"empty", loop_empty, ITERATIONS},
    {"alu", loop_alu, ITERATIONS},
    {"ldr", loop_ldr, ITERATIONS},
    {"ldr_alu", loop_ldr_alu, ITERATIONS},
    {"ldr_flash", loop_ldr_flash, ITERATIONS},
    {"str", loop_str, ITERATIONS},
    {"mul", loop_mul, ITERATIONS},
    {"udiv_small", loop_udiv_small, ITERATIONS},
    {"udiv_large", loop_udiv_large, ITERATIONS},
    {"branch", loop_branch, ITERATIONS},
    {"call", loop_call, ITERATIONS},
    {"ldm", loop_ldm, ITERATIONS},
    {"push_pop", loop_push_pop, ITERATIONS},
    {"smlad", loop_smlad, ITERATIONS},
    {"vadd", loop_vadd, ITERATIONS},
    {"vmul", loop_vmul, ITERATIONS},
    {"vfma", loop_vfma, ITERATIONS},
    {"vdiv", loop_vdiv, ITERATIONS},
    {"vsqrt", loop_vsqrt, ITERATIONS},
    {"vldr", loop_vldr, ITERATIONS},
    {"vmov", loop_vmov, ITERATIONS},
    {"long", loop_long, ITERATIONS / 100u},
    {"c_sort", c_sort, ITERATIONS},
    {"c_crc", c_crc, ITERATIONS},
    {"c_filter", c_filter, ITERATIONS},
    {"irq", c_irq, ITERATIONS},
    {"irq_fpu", c_irq_fpu, ITERATIONS},
};
#define TESTS (sizeof(tests) / sizeof(tests[0]))
#define SETUPS 4u

static uint32_t results[SETUPS][TESTS];

static void measure(uint32_t *out)
{
    for (uint32_t t = 0; t < TESTS; ++t) {
        tests[t].run(tests[t].iterations); // once to fill the caches and the prefetch buffer
        out[t] = tests[t].run(tests[t].iterations);
    }
}

// 168 MHz from the 25 MHz crystal, as f4_clock: 5 wait states first.
static void clock_168mhz(void)
{
    usart1_flush();
    RCC_CR |= 1u << 16; // HSEON
    while (!(RCC_CR & (1u << 17))) { // HSERDY: waits for ever without a crystal
    }
    FLASH_ACR = 5u | ACR_PRFTEN | ACR_ICEN | ACR_DCEN;
    RCC_PLLCFGR = (HSE_VALUE / 1000000u) | (336u << 6) | (0u << 16) | (1u << 22) | (7u << 24);
    RCC_CR |= 1u << 24; // PLLON
    while (!(RCC_CR & (1u << 25))) {
    }
    RCC_CFGR = (5u << 10) | (4u << 13); // PCLK1 = HCLK / 4, PCLK2 = HCLK / 2 (84 MHz)
    RCC_CFGR |= 2u;                      // SW: the PLL
    while (((RCC_CFGR >> 2) & 3u) != 2u) {
    }
    USART1_BRR = (84000000u + 115200u / 2u) / 115200u;
}

int main(void)
{
    usart1_start((16000000u + 115200u / 2u) / 115200u);
    DEMCR |= 1u << 24; // TRCENA
    DWT_CYCCNT = 0;
    DWT_CTRL |= 1u; // CYCCNTENA
    put_text("f4_timing: measuring...\r\n");
    FLASH_ACR = 0u;
    measure(results[0]);
    clock_168mhz();
    measure(results[1]);
    FLASH_ACR = 5u | ACR_ICEN | ACR_DCEN;
    measure(results[2]);
    FLASH_ACR = 5u;
    measure(results[3]);
    static const char *const setups[SETUPS] = {
        "16 MHz HSI, 0 wait states, ART off",
        "168 MHz, 5 wait states, prefetch and caches",
        "168 MHz, 5 wait states, caches without prefetch",
        "168 MHz, 5 wait states, ART off",
    };
    for (;;) {
        put_text("\r\nf4_timing v1\r\n");
        for (uint32_t s = 0; s < SETUPS; ++s) {
            put_text("# ");
            put_text(setups[s]);
            put_text("\r\n");
            for (uint32_t t = 0; t < TESTS; ++t) {
                put_text(tests[t].name);
                put_char(' ');
                put_decimal(results[s][t]);
                put_text("\r\n");
            }
        }
        put_text("end\r\n");
        const uint32_t start = DWT_CYCCNT;
        while (DWT_CYCCNT - start < 168000000u * 3u) { // about 3 s
        }
    }
}
