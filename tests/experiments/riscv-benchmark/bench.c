// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// Benchmark mix for the RV32IMC core (E-08, gate G8a.3): the same integer
// work as tests/experiments/emu-benchmark/bench.c (CRC32, memory copies,
// sorting, division, recursion) with a 1 kHz timer interrupt, never sleeping,
// on SimNodus's RISC-V test machine or QEMU's virt board (CLINT timer at
// 10 MHz, 16550 UART). The UART prints a checksum per round so that two
// emulators can be compared.
#include <stdint.h>

#define REG(address) (*(volatile uint32_t *)(address))
#define UART_THR (*(volatile uint8_t *)0x10000000u)
#define MTIMECMP_LO REG(0x02004000u)
#define MTIMECMP_HI REG(0x02004004u)
#define MTIME_LO REG(0x0200BFF8u)
#define TICK 10000u // 1 ms at 10 MHz

extern uint32_t _bss_start, _bss_end, _stack_top;

int main(void);

__attribute__((naked, section(".text.start"))) void _start(void)
{
    __asm__ volatile(
        "la sp, _stack_top\n"
        "j main\n");
}

static volatile uint32_t ticks;
static uint32_t next_tick;
static uint32_t buffer_a[256];
static uint32_t buffer_b[256];

__attribute__((interrupt("machine"), aligned(4))) static void trap_handler(void)
{
    // Only the machine timer is enabled.
    next_tick += TICK;
    MTIMECMP_LO = next_tick;
    ++ticks;
}

static uint32_t crc32(const uint8_t* data, uint32_t length)
{
    uint32_t crc = 0xFFFFFFFFu;
    for (uint32_t i = 0; i < length; ++i) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; ++bit) {
            crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
        }
    }
    return ~crc;
}

static void copy(uint32_t* to, const uint32_t* from, uint32_t words)
{
    while (words--) {
        *to++ = *from++;
    }
}

static void sort(uint32_t* values, uint32_t count)
{
    for (uint32_t i = 1; i < count; ++i) {
        const uint32_t v = values[i];
        uint32_t j = i;
        while (j > 0 && values[j - 1] > v) {
            values[j] = values[j - 1];
            --j;
        }
        values[j] = v;
    }
}

static uint32_t fib(uint32_t n)
{
    return n < 2 ? n : fib(n - 1) + fib(n - 2);
}

static uint32_t divide_mix(uint32_t seed)
{
    uint32_t sum = 0;
    for (uint32_t i = 1; i < 200; ++i) {
        sum += seed / i + (int32_t)seed % (int32_t)(i * 3 + 1);
    }
    return sum;
}

static void put(char c)
{
    UART_THR = (uint8_t)c;
}

static void put_hex(uint32_t value)
{
    for (int shift = 28; shift >= 0; shift -= 4) {
        const uint32_t digit = (value >> shift) & 0xFu;
        put((char)(digit < 10 ? '0' + digit : 'A' + digit - 10));
    }
    put('\r');
    put('\n');
}

int main(void)
{
    for (uint32_t* dst = &_bss_start; dst < &_bss_end;) {
        *dst++ = 0;
    }
    __asm__ volatile("csrw mtvec, %0" : : "r"((uintptr_t)trap_handler));
    next_tick = MTIME_LO + TICK;
    MTIMECMP_HI = 0;
    MTIMECMP_LO = next_tick;
    __asm__ volatile("csrs mie, %0" : : "r"(1u << 7));
    __asm__ volatile("csrsi mstatus, 8");

    uint32_t seed = 12345;
    for (uint32_t round = 0;; ++round) {
        for (uint32_t i = 0; i < 256; ++i) {
            seed = seed * 1103515245u + 12345u;
            buffer_a[i] = seed;
        }
        copy(buffer_b, buffer_a, 256);
        sort(buffer_b, 256);
        uint32_t check = crc32((const uint8_t*)buffer_b, sizeof buffer_b);
        check ^= fib(18);
        check += divide_mix(seed);
        put_hex(check ^ round);
    }
}
