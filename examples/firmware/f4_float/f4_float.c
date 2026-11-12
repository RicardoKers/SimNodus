// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// STM32F407 floating point and DSP instructions (experiment E-12, gate
// G12b.5): the same numbers must come out of SimNodus and of a real chip.
//
// Three workloads, each folded into a checksum of the bits of its results:
//   A  single-precision arithmetic: multiply, add, divide, square root, a
//      fused multiply-add, comparisons and conversions to and from integers;
//   B  a second-order filter over a sawtooth, as a control loop would run;
//   C  DSP instructions (saturating and parallel arithmetic, SEL, the dual
//      and halfword multiplies, UMAAL) and the Q flag.
// A SysTick interrupt every millisecond does floating-point work of its own
// while the main program does: the lazy stacking of the FPU's registers must
// keep both intact. Its accumulator at the 20th tick is printed too.
//
// Output on USART1 (PA9, 115200 baud at the internal 16 MHz clock):
//   A <checksum>  B <checksum>  C <checksum>  I <bits>  x <bits>
// as hexadecimal words, one per line, then "done".
#define F4_SYSTICK SysTick_Handler
#include "../f4_common/f4.h"

static volatile uint32_t ticks;
static volatile float interrupt_value = 1.0f;
static volatile uint32_t interrupt_at_20;

static uint32_t bits_of(float value)
{
    union {
        float f;
        uint32_t u;
    } both = {value};
    return both.u;
}

void SysTick_Handler(void)
{
    // Floating point in the handler: S0 to S15 of the interrupted code are saved lazily.
    const uint32_t now = ++ticks;
    float value = interrupt_value;
    value = value * 1.5f + (float)now;
    if (value > 1.0e6f) {
        value = value / 1024.0f;
    }
    interrupt_value = value;
    if (now == 20u) {
        interrupt_at_20 = bits_of(value);
    }
}

static uint32_t fold(uint32_t checksum, uint32_t word)
{
    return (checksum ^ word) * 16777619u; // FNV-1a over words
}

static uint32_t workload_a(float *last)
{
    uint32_t checksum = 2166136261u;
    float x = 0.1f;
    float z = 0.0f;
    for (uint32_t i = 0; i < 20000u; ++i) {
        x = x * 1.0001f + 0.25f / (float)(i + 1u);
        const float root = __builtin_sqrtf(x);
        z = __builtin_fmaf(x, root, z); // one rounding
        if (z > 1000.0f) {
            z = z - 1000.0f;
        }
        const int32_t thousandths = (int32_t)(x * 1000.0f);        // toward zero
        const uint32_t scaled = (uint32_t)(root * 65536.0f);
        const float back = (float)thousandths / 1000.0f - (float)scaled;
        checksum = fold(checksum, bits_of(x));
        checksum = fold(checksum, bits_of(root));
        checksum = fold(checksum, bits_of(z));
        checksum = fold(checksum, (uint32_t)thousandths);
        checksum = fold(checksum, scaled);
        checksum = fold(checksum, bits_of(back));
        if (x > 8.0f) {
            x = x * 0.125f;
        }
    }
    *last = x;
    return checksum;
}

static uint32_t workload_b(void)
{
    // A low-pass biquad (direct form 1) over a sawtooth with a step.
    const float b0 = 0.0675f, b1 = 0.135f, b2 = 0.0675f, a1 = -1.143f, a2 = 0.4128f;
    float x1 = 0, x2 = 0, y1 = 0, y2 = 0;
    uint32_t checksum = 2166136261u;
    for (uint32_t i = 0; i < 4000u; ++i) {
        const float input = (float)(i % 97u) * 0.01f - (i > 2000u ? 0.3f : 0.0f);
        const float output = b0 * input + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
        x2 = x1;
        x1 = input;
        y2 = y1;
        y1 = output;
        checksum = fold(checksum, bits_of(output));
    }
    return checksum;
}

#define DSP3(name, instruction)                                                       \
    static inline uint32_t name(uint32_t a, uint32_t b)                               \
    {                                                                                 \
        uint32_t result;                                                              \
        __asm__ volatile(instruction " %0, %1, %2" : "=r"(result) : "r"(a), "r"(b)); \
        return result;                                                                \
    }
DSP3(qadd, "qadd")
DSP3(qdsub, "qdsub")
DSP3(sadd16, "sadd16")
DSP3(uqsub8, "uqsub8")
DSP3(shasx, "shasx")
DSP3(smulbt, "smulbt")
DSP3(smuad, "smuad")
DSP3(smmulr, "smmulr")
DSP3(sel, "sel")

static uint32_t workload_c(void)
{
    uint32_t checksum = 2166136261u;
    uint32_t a = 0x12345678u;
    uint32_t b = 0x9ABCDEF0u;
    for (uint32_t i = 0; i < 5000u; ++i) {
        a = a * 1664525u + 1013904223u; // the same numbers on every run
        b = b * 22695477u + 1u;
        checksum = fold(checksum, qadd(a, b));
        checksum = fold(checksum, qdsub(a, b));
        const uint32_t sums = sadd16(a, b); // sets the GE flags that SEL uses
        checksum = fold(checksum, sums);
        checksum = fold(checksum, sel(a, b));
        checksum = fold(checksum, uqsub8(a, b));
        checksum = fold(checksum, shasx(a, b));
        checksum = fold(checksum, smulbt(a, b));
        checksum = fold(checksum, smuad(a, b));
        checksum = fold(checksum, smmulr(a, b));
        uint32_t low = a, high = b;
        __asm__ volatile("umaal %0, %1, %2, %3" : "+r"(low), "+r"(high) : "r"(a), "r"(b));
        checksum = fold(checksum, low);
        checksum = fold(checksum, high);
        uint32_t packed, saturated;
        __asm__ volatile("pkhbt %0, %1, %2, lsl #8" : "=r"(packed) : "r"(a), "r"(b));
        __asm__ volatile("ssat16 %0, #9, %1" : "=r"(saturated) : "r"(a));
        checksum = fold(checksum, packed);
        checksum = fold(checksum, saturated);
    }
    uint32_t apsr;
    __asm__ volatile("mrs %0, APSR" : "=r"(apsr));
    return fold(checksum, apsr & (1u << 27)); // Q: something saturated
}

static void put_line(const char *label, uint32_t value)
{
    put_text(label);
    put_char(' ');
    put_hex(value);
    put_text("\r\n");
}

int main(void)
{
    usart1_start((16000000u + 115200u / 2u) / 115200u);
    SYST_RVR = 16000u - 1u;
    SYST_CVR = 0;
    SYST_CSR = 7u;
    float last = 0;
    const uint32_t a = workload_a(&last);
    const uint32_t b = workload_b();
    const uint32_t c = workload_c();
    while (ticks < 20u) {
    }
    put_line("A", a);
    put_line("B", b);
    put_line("C", c);
    put_line("I", interrupt_at_20);
    put_line("x", bits_of(last));
    put_text("done\r\n");
    usart1_flush();
    for (;;) {
    }
}
