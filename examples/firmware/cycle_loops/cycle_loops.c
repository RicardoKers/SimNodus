// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// Cycle measurements for the Cortex-M3 timing model (SN-181), for a real Blue
// Pill and for SimNodus: each test runs a loop and counts its core cycles with
// DWT CYCCNT. The tests run twice, at 8 MHz from the internal oscillator with
// no flash wait state, and at 72 MHz from the 8 MHz crystal with two wait
// states (as STM32CubeMX sets it up). The results are printed on USART1
// (PA9, 115200 baud) every few seconds as "name cycles" lines, so a serial
// terminal can join at any time.
#include <stdint.h>

#define REG(address) (*(volatile uint32_t *)(address))
#define RCC_CR REG(0x40021000u)
#define RCC_CFGR REG(0x40021004u)
#define RCC_APB2ENR REG(0x40021018u)
#define FLASH_ACR REG(0x40022000u)
#define GPIOA_CRH REG(0x40010804u)
#define USART1_SR REG(0x40013800u)
#define USART1_DR REG(0x40013804u)
#define USART1_BRR REG(0x40013808u)
#define USART1_CR1 REG(0x4001380Cu)
#define SCB_VTOR REG(0xE000ED08u)
#define SCB_ICSR REG(0xE000ED04u)
#define DEMCR REG(0xE000EDFCu)
#define DWT_CTRL REG(0xE0001000u)
#define DWT_CYCCNT REG(0xE0001004u)
#define DWT_LAR REG(0xE0001FB0u)

#define SR_TXE (1u << 7u)
#define SR_TC (1u << 6u)
#define ITERATIONS 1000u

extern uint32_t _data_load, _data_start, _data_end, _bss_start, _bss_end, _stack_top;

void Reset_Handler(void);
void Default_Handler(void);
void PendSV_Handler(void);

__attribute__((section(".vectors"), used))
const uintptr_t vectors[16 + 38] = {
    [0] = (uintptr_t)&_stack_top,
    [1] = (uintptr_t)Reset_Handler,
    [2 ... 13] = (uintptr_t)Default_Handler,
    [14] = (uintptr_t)PendSV_Handler,
    [15 ... 53] = (uintptr_t)Default_Handler,
};

void Default_Handler(void)
{
    for (;;) {
    }
}

volatile uint32_t scratch[8] = {1, 2, 3, 4, 5, 6, 7, 8};
static volatile uint32_t pendsv_count;

void PendSV_Handler(void)
{
    ++pendsv_count;
}

// Each loop takes the iteration count in r0 and returns the cycles it took.
// r3 points at CYCCNT, r6 at `scratch`; the body is eight instructions under
// test, then "subs r0, #1; bne 1b". Loops start on an 8-byte boundary.
#define X4(i) i "\n" i "\n" i "\n" i "\n"
#define X8(i) X4(i) X4(i)
#define LOOP(name, setup, body)                                                                                      \
    ".balign 8\n.global " #name "\n.type " #name ", %function\n.thumb_func\n" #name ":\n"                          \
    "push {r4-r7, lr}\nldr r3, =0xE0001004\nldr r6, =scratch\n" setup "\nldr r4, [r3]\n.balign 8\n1:\n" body      \
    "subs r0, #1\nbne 1b\nldr r5, [r3]\nsubs r0, r5, r4\npop {r4-r7, pc}\n.ltorg\n"

__asm__(".syntax unified\n.thumb\n.section .text.loops,\"ax\",%progbits\n"
        ".balign 4\n.thumb_func\nreturn_only:\nbx lr\n"
        LOOP(loop_empty, "", "")
        LOOP(loop_alu, "movs r1, #0\nmovs r2, #3", X8("adds r1, r1, r2"))
        LOOP(loop_ldr, "", X8("ldr r1, [r6]"))
        LOOP(loop_ldr_alu, "movs r2, #0", X4("ldr r1, [r6]\nadds r2, r2, r1"))
        LOOP(loop_str, "movs r1, #5", X8("str r1, [r6]"))
        LOOP(loop_mul, "movs r1, #3\nmovs r2, #5", X8("muls r1, r2, r1"))
        LOOP(loop_udiv_small, "movs r2, #100\nmovs r7, #7", X8("udiv r1, r2, r7"))
        LOOP(loop_udiv_large, "movs r2, #0\nsubs r2, #1\nmovs r7, #3", X8("udiv r1, r2, r7"))
        LOOP(loop_branch, "", X8("b 2f\n2:"))
        LOOP(loop_call, "", X8("bl return_only"))
        LOOP(loop_ldm, "", X8("ldmia r6, {r1, r2, r5, r7}"))
        LOOP(loop_push_pop, "", X4("push {r1, r2, r5, r7}\npop {r1, r2, r5, r7}")));

uint32_t loop_empty(uint32_t n);
uint32_t loop_alu(uint32_t n);
uint32_t loop_ldr(uint32_t n);
uint32_t loop_ldr_alu(uint32_t n);
uint32_t loop_str(uint32_t n);
uint32_t loop_mul(uint32_t n);
uint32_t loop_udiv_small(uint32_t n);
uint32_t loop_udiv_large(uint32_t n);
uint32_t loop_branch(uint32_t n);
uint32_t loop_call(uint32_t n);
uint32_t loop_ldm(uint32_t n);
uint32_t loop_push_pop(uint32_t n);

// Plain C, as a compiler writes it (-Os): bubble sort and a bitwise CRC-32.
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

// Exception entry and return: PendSV set n / 10 times.
static uint32_t c_irq(uint32_t n)
{
    const uint32_t start = DWT_CYCCNT;
    for (uint32_t i = 0; i < n / 10u; ++i) {
        SCB_ICSR = 1u << 28u; // PENDSVSET
    }
    return DWT_CYCCNT - start;
}

typedef uint32_t (*Test)(uint32_t);
static const struct {
    const char *name;
    Test run;
} tests[] = {
    {"empty", loop_empty},       {"alu", loop_alu},
    {"ldr", loop_ldr},           {"ldr_alu", loop_ldr_alu},
    {"str", loop_str},           {"mul", loop_mul},
    {"udiv_small", loop_udiv_small}, {"udiv_large", loop_udiv_large},
    {"branch", loop_branch},     {"call", loop_call},
    {"ldm", loop_ldm},           {"push_pop", loop_push_pop},
    {"c_sort", c_sort},          {"c_crc", c_crc},
    {"irq", c_irq},
};
#define TESTS (sizeof(tests) / sizeof(tests[0]))

static uint32_t results[2][TESTS];

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

static void measure(uint32_t *out)
{
    for (uint32_t t = 0; t < TESTS; ++t) {
        tests[t].run(ITERATIONS); // once to settle the prefetch buffer
        out[t] = tests[t].run(ITERATIONS);
    }
}

// 72 MHz from the 8 MHz crystal: two flash wait states first, APB1 at 36 MHz.
static void clock_72mhz(void)
{
    while (!(USART1_SR & SR_TC)) {
    }
    RCC_CR |= 1u << 16u; // HSEON
    while (!(RCC_CR & (1u << 17u))) {
    }
    FLASH_ACR = (1u << 4u) | 2u;                          // prefetch on, two wait states
    RCC_CFGR = (7u << 18u) | (1u << 16u) | (4u << 8u);   // PLL x9 from HSE, APB1 / 2
    RCC_CR |= 1u << 24u;                                  // PLLON
    while (!(RCC_CR & (1u << 25u))) {
    }
    RCC_CFGR |= 2u; // SYSCLK from the PLL
    while ((RCC_CFGR & (3u << 2u)) != (2u << 2u)) {
    }
    USART1_BRR = 625u; // 72 MHz / 115200
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
    GPIOA_CRH = (GPIOA_CRH & ~0xFF0u) | (0xBu << 4u) | (0x4u << 8u);
    USART1_BRR = 69u; // 8 MHz / 115200
    USART1_CR1 = (1u << 13u) | (1u << 3u); // UE, TE
    DEMCR |= 1u << 24u; // TRCENA
    DWT_LAR = 0xC5ACCE55u;
    DWT_CYCCNT = 0;
    DWT_CTRL |= 1u; // CYCCNTENA
    put_string("cycle_loops: measuring...\r\n");
    measure(results[0]);
    clock_72mhz();
    measure(results[1]);
    static const char *const clocks[2] = {"8 MHz, 0 wait states", "72 MHz, 2 wait states"};
    for (;;) {
        put_string("\r\ncycle_loops v1, ");
        put_number(ITERATIONS);
        put_string(" iterations\r\n");
        for (int c = 0; c < 2; ++c) {
            put_string("# ");
            put_string(clocks[c]);
            put_string("\r\n");
            for (uint32_t t = 0; t < TESTS; ++t) {
                put_string(tests[t].name);
                put_char(' ');
                put_number(results[c][t]);
                put_string("\r\n");
            }
        }
        put_string("end\r\n");
        const uint32_t start = DWT_CYCCNT;
        while (DWT_CYCCNT - start < 72000000u * 3u) { // about 3 s
        }
    }
}
