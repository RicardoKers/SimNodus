// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// AVRe+ core of experiment E-10 (SN-256, gate G10a): every instruction of the
// ATmega8 with its result, its flags and its cycles from the AVR Instruction
// Set Manual (DS40002198C), interrupts, decoding of every opcode, random
// images, and the differential corpus recorded from avr8js
// (tests/experiments/avr-differential).
//
//   avr_tests <repository-dir> [filter]
#include "test_support.hpp"

#include "emu/atmega.hpp"
#include "emu/avr_cpu.hpp"
#include "emu/elf.hpp"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <optional>
#include <random>
#include <string>
#include <vector>

using namespace simnodus::emu;
namespace fs = std::filesystem;

namespace {

fs::path repository;
// avr_tests <repository> "avr-diff" <block> prints that block's steps, to
// compare with `node run_avr8js.js <modules> <block>`.
long trace_block = -1;

// ---- Encoders (AVR Instruction Set Manual, opcode column) ----
std::uint16_t rr(std::uint16_t base, int d, int r)
{
    return static_cast<std::uint16_t>(base | ((d & 0x1F) << 4) | (r & 0x0F) | ((r & 0x10) << 5));
}
std::uint16_t imm(std::uint16_t base, int d, int k)
{
    return static_cast<std::uint16_t>(base | (((d - 16) & 0x0F) << 4) | (k & 0x0F) | ((k & 0xF0) << 4));
}
std::uint16_t one(std::uint16_t base, int d) { return static_cast<std::uint16_t>(base | ((d & 0x1F) << 4)); }

std::uint16_t ADD(int d, int r) { return rr(0x0C00, d, r); }
std::uint16_t ADC(int d, int r) { return rr(0x1C00, d, r); }
std::uint16_t SUB(int d, int r) { return rr(0x1800, d, r); }
std::uint16_t SBC(int d, int r) { return rr(0x0800, d, r); }
std::uint16_t CP(int d, int r) { return rr(0x1400, d, r); }
std::uint16_t CPC(int d, int r) { return rr(0x0400, d, r); }
std::uint16_t CPSE(int d, int r) { return rr(0x1000, d, r); }
std::uint16_t AND(int d, int r) { return rr(0x2000, d, r); }
std::uint16_t EOR(int d, int r) { return rr(0x2400, d, r); }
std::uint16_t OR(int d, int r) { return rr(0x2800, d, r); }
std::uint16_t MOV(int d, int r) { return rr(0x2C00, d, r); }
std::uint16_t MUL(int d, int r) { return rr(0x9C00, d, r); }
std::uint16_t MULS(int d, int r) { return static_cast<std::uint16_t>(0x0200 | ((d - 16) << 4) | (r - 16)); }
std::uint16_t mul3(std::uint16_t base, int d, int r) { return static_cast<std::uint16_t>(base | ((d - 16) << 4) | (r - 16)); }
std::uint16_t MULSU(int d, int r) { return mul3(0x0300, d, r); }
std::uint16_t FMUL(int d, int r) { return mul3(0x0308, d, r); }
std::uint16_t FMULS(int d, int r) { return mul3(0x0380, d, r); }
std::uint16_t FMULSU(int d, int r) { return mul3(0x0388, d, r); }
std::uint16_t MOVW(int d, int r) { return static_cast<std::uint16_t>(0x0100 | ((d / 2) << 4) | (r / 2)); }
std::uint16_t LDI(int d, int k) { return imm(0xE000, d, k); }
std::uint16_t CPI(int d, int k) { return imm(0x3000, d, k); }
std::uint16_t SBCI(int d, int k) { return imm(0x4000, d, k); }
std::uint16_t SUBI(int d, int k) { return imm(0x5000, d, k); }
std::uint16_t ORI(int d, int k) { return imm(0x6000, d, k); }
std::uint16_t ANDI(int d, int k) { return imm(0x7000, d, k); }
std::uint16_t COM(int d) { return one(0x9400, d); }
std::uint16_t NEG(int d) { return one(0x9401, d); }
std::uint16_t SWAP(int d) { return one(0x9402, d); }
std::uint16_t INC(int d) { return one(0x9403, d); }
std::uint16_t ASR(int d) { return one(0x9405, d); }
std::uint16_t LSR(int d) { return one(0x9406, d); }
std::uint16_t ROR(int d) { return one(0x9407, d); }
std::uint16_t DEC(int d) { return one(0x940A, d); }
std::uint16_t PUSH(int d) { return one(0x920F, d); }
std::uint16_t POP(int d) { return one(0x900F, d); }
std::uint16_t LDS(int d) { return one(0x9000, d); }
std::uint16_t STS(int d) { return one(0x9200, d); }
std::uint16_t LD_X(int d) { return one(0x900C, d); }
std::uint16_t LD_XP(int d) { return one(0x900D, d); }
std::uint16_t LD_MX(int d) { return one(0x900E, d); }
std::uint16_t LD_YP(int d) { return one(0x9009, d); }
std::uint16_t LD_MY(int d) { return one(0x900A, d); }
std::uint16_t LD_ZP(int d) { return one(0x9001, d); }
std::uint16_t LD_MZ(int d) { return one(0x9002, d); }
std::uint16_t ST_X(int d) { return one(0x920C, d); }
std::uint16_t ST_XP(int d) { return one(0x920D, d); }
std::uint16_t ST_MX(int d) { return one(0x920E, d); }
std::uint16_t ST_YP(int d) { return one(0x9209, d); }
std::uint16_t ST_MY(int d) { return one(0x920A, d); }
std::uint16_t ST_ZP(int d) { return one(0x9201, d); }
std::uint16_t ST_MZ(int d) { return one(0x9202, d); }
std::uint16_t LPM_Z(int d) { return one(0x9004, d); }
std::uint16_t LPM_ZP(int d) { return one(0x9005, d); }
std::uint16_t ldd(bool store, bool y, int d, int q)
{
    return static_cast<std::uint16_t>(0x8000 | (store ? 0x0200 : 0) | (y ? 0x0008 : 0) | ((d & 0x1F) << 4) | (q & 7)
                                      | ((q & 0x18) << 7) | ((q & 0x20) << 8));
}
std::uint16_t ADIW(int d, int k) { return static_cast<std::uint16_t>(0x9600 | (((d - 24) / 2) << 4) | (k & 0x0F) | ((k & 0x30) << 2)); }
std::uint16_t SBIW(int d, int k) { return static_cast<std::uint16_t>(0x9700 | (((d - 24) / 2) << 4) | (k & 0x0F) | ((k & 0x30) << 2)); }
std::uint16_t io(std::uint16_t base, int a, int b) { return static_cast<std::uint16_t>(base | (a << 3) | b); }
std::uint16_t CBI(int a, int b) { return io(0x9800, a, b); }
std::uint16_t SBIC(int a, int b) { return io(0x9900, a, b); }
std::uint16_t SBI(int a, int b) { return io(0x9A00, a, b); }
std::uint16_t SBIS(int a, int b) { return io(0x9B00, a, b); }
std::uint16_t IN(int d, int a) { return static_cast<std::uint16_t>(0xB000 | ((d & 0x1F) << 4) | (a & 0x0F) | ((a & 0x30) << 5)); }
std::uint16_t OUT(int a, int r) { return static_cast<std::uint16_t>(0xB800 | ((r & 0x1F) << 4) | (a & 0x0F) | ((a & 0x30) << 5)); }
std::uint16_t RJMP(int k) { return static_cast<std::uint16_t>(0xC000 | (k & 0x0FFF)); }
std::uint16_t RCALL(int k) { return static_cast<std::uint16_t>(0xD000 | (k & 0x0FFF)); }
std::uint16_t BRBS(int s, int k) { return static_cast<std::uint16_t>(0xF000 | ((k & 0x7F) << 3) | s); }
std::uint16_t BRBC(int s, int k) { return static_cast<std::uint16_t>(0xF400 | ((k & 0x7F) << 3) | s); }
std::uint16_t BLD(int d, int b) { return static_cast<std::uint16_t>(0xF800 | ((d & 0x1F) << 4) | b); }
std::uint16_t BST(int d, int b) { return static_cast<std::uint16_t>(0xFA00 | ((d & 0x1F) << 4) | b); }
std::uint16_t SBRC(int r, int b) { return static_cast<std::uint16_t>(0xFC00 | ((r & 0x1F) << 4) | b); }
std::uint16_t SBRS(int r, int b) { return static_cast<std::uint16_t>(0xFE00 | ((r & 0x1F) << 4) | b); }
std::uint16_t BSET(int s) { return static_cast<std::uint16_t>(0x9408 | (s << 4)); }
std::uint16_t BCLR(int s) { return static_cast<std::uint16_t>(0x9488 | (s << 4)); }
constexpr std::uint16_t NOP = 0x0000, RET = 0x9508, RETI = 0x9518, SLEEP = 0x9588, WDR = 0x95A8, LPM = 0x95C8,
                        SPM = 0x95E8, IJMP = 0x9409, ICALL = 0x9509, BREAK = 0x9598;

constexpr std::uint8_t C = 1, Zf = 2, N = 4, V = 8, S = 16, H = 32, T = 64, I = 128;

// An ATmega8 core with a program at 0.
struct Bench {
    AvrCpu cpu;
    std::vector<std::string> diagnostics;
    explicit Bench(std::vector<std::uint16_t> program, const AvrConfig& config = {}) : cpu(config)
    {
        cpu.load_flash(program);
        cpu.diagnostic = [this](const std::string& text) { diagnostics.push_back(text); };
        cpu.set_sp(0x45F);
    }
    // Runs `n` instructions; returns the cycles they took.
    std::uint64_t step(int n = 1)
    {
        const std::uint64_t before = cpu.cycles();
        for (int i = 0; i < n; ++i) {
            cpu.step_instruction();
        }
        return cpu.cycles() - before;
    }
    std::uint8_t flags() const { return static_cast<std::uint8_t>(cpu.sreg() & 0x3F); }
};

// One two-operand instruction on r16, r17 with the carry in `sreg`; checks the
// result in r16 and the six arithmetic flags.
void check_rr(std::uint16_t insn, std::uint8_t a, std::uint8_t b, std::uint8_t sreg, std::uint8_t result,
    std::uint8_t flags, const char* what)
{
    Bench t({insn});
    t.cpu.set_reg(16, a);
    t.cpu.set_reg(17, b);
    t.cpu.set_sreg(sreg);
    const std::uint64_t cycles = t.step();
    if (t.cpu.reg(16) != result || t.flags() != flags || cycles != 1) {
        char text[200];
        std::snprintf(text, sizeof text, "%s 0x%02X,0x%02X sreg 0x%02X: got r16=0x%02X flags=0x%02X cycles=%u, expected 0x%02X 0x%02X 1",
            what, a, b, sreg, t.cpu.reg(16), t.flags(), static_cast<unsigned>(cycles), result, flags);
        simnodus::test::fail(__FILE__, __LINE__, text);
    }
}

void check_one(std::uint16_t insn, std::uint8_t a, std::uint8_t sreg, std::uint8_t result, std::uint8_t flags, const char* what)
{
    Bench t({insn});
    t.cpu.set_reg(16, a);
    t.cpu.set_sreg(sreg);
    const std::uint64_t cycles = t.step();
    if (t.cpu.reg(16) != result || t.flags() != flags || cycles != 1) {
        char text[200];
        std::snprintf(text, sizeof text, "%s 0x%02X sreg 0x%02X: got 0x%02X flags=0x%02X cycles=%u, expected 0x%02X 0x%02X",
            what, a, sreg, t.cpu.reg(16), t.flags(), static_cast<unsigned>(cycles), result, flags);
        simnodus::test::fail(__FILE__, __LINE__, text);
    }
}

std::vector<std::uint8_t> read_file(const fs::path& path)
{
    std::ifstream in(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

} // namespace

// ---------------------------------------------------------------------------
// Gate G10a.1: results, flags and cycles.

TEST_CASE("avr: ADD and ADC, with the half carry from bit 3")
{
    check_rr(ADD(16, 17), 5, 5, 0, 10, 0, "ADD"); // no carry out of bit 3
    check_rr(ADD(16, 17), 8, 8, 0, 16, H, "ADD");
    check_rr(ADD(16, 17), 0x80, 0x80, 0, 0, C | Zf | V | S, "ADD");
    check_rr(ADD(16, 17), 0x7F, 0x01, 0, 0x80, H | V | N, "ADD");
    check_rr(ADD(16, 17), 0xFF, 0x01, 0, 0, C | Zf | H, "ADD");
    check_rr(ADC(16, 17), 0xFF, 0x00, C, 0, C | Zf | H, "ADC");
    check_rr(ADC(16, 17), 0x0E, 0x01, C, 0x10, H, "ADC");
    check_rr(ADC(16, 17), 0x10, 0x10, 0, 0x20, 0, "ADC");
}

TEST_CASE("avr: SUB, SBC, CP, CPC and the zero flag carried through")
{
    check_rr(SUB(16, 17), 0x00, 0x01, 0, 0xFF, C | H | N | S, "SUB");
    check_rr(SUB(16, 17), 0x80, 0x01, 0, 0x7F, H | V | S, "SUB");
    check_rr(SUB(16, 17), 0x10, 0x10, 0, 0, Zf, "SUB");
    check_rr(SBC(16, 17), 0x10, 0x0F, C, 0, H, "SBC"); // Z was clear: stays clear
    check_rr(SBC(16, 17), 0x10, 0x0F, C | Zf, 0, Zf | H, "SBC");
    check_rr(SBC(16, 17), 0x00, 0x00, C, 0xFF, C | H | N | S, "SBC");
    check_rr(CP(16, 17), 0x05, 0x07, 0, 0x05, C | H | N | S, "CP");
    check_rr(CPC(16, 17), 0x05, 0x05, Zf, 0x05, Zf, "CPC");
    check_rr(CPC(16, 17), 0x05, 0x05, Zf | C, 0x05, C | H | N | S, "CPC");
    check_rr(EOR(16, 17), 0xF0, 0xFF, C, 0x0F, C, "EOR"); // C kept, V cleared
    check_rr(AND(16, 17), 0xF0, 0x80, V, 0x80, N | S, "AND");
    check_rr(OR(16, 17), 0x00, 0x00, V, 0x00, Zf, "OR");
    check_rr(MOV(16, 17), 0x00, 0x5A, 0x3F, 0x5A, 0x3F, "MOV");
}

TEST_CASE("avr: immediate forms")
{
    check_one(SUBI(16, 0x01), 0x00, 0, 0xFF, C | H | N | S, "SUBI");
    check_one(SBCI(16, 0x00), 0x00, C | Zf, 0xFF, C | H | N | S, "SBCI");
    check_one(SBCI(16, 0x01), 0x02, C | Zf, 0x00, Zf, "SBCI");
    check_one(CPI(16, 0x80), 0x7F, 0, 0x7F, C | V | N, "CPI");
    check_one(ANDI(16, 0x0F), 0xF3, V, 0x03, 0, "ANDI");
    check_one(ORI(16, 0x80), 0x01, 0, 0x81, N | S, "ORI");
    check_one(LDI(16, 0xA5), 0x00, 0x3F, 0xA5, 0x3F, "LDI");
}

TEST_CASE("avr: one-operand instructions")
{
    check_one(COM(16), 0x0F, 0, 0xF0, C | N | S, "COM");
    check_one(COM(16), 0xFF, 0, 0x00, C | Zf, "COM");
    check_one(NEG(16), 0x80, 0, 0x80, C | V | N, "NEG"); // N and V: S = 0
    check_one(NEG(16), 0x00, 0, 0x00, Zf, "NEG");
    check_one(NEG(16), 0x01, 0, 0xFF, C | H | N | S, "NEG");
    check_one(NEG(16), 0x10, 0, 0xF0, C | N | S, "NEG");
    check_one(INC(16), 0x7F, C | H, 0x80, C | H | V | N, "INC");
    check_one(INC(16), 0xFF, 0, 0x00, Zf, "INC");
    check_one(DEC(16), 0x80, C, 0x7F, C | V | S, "DEC");
    check_one(DEC(16), 0x01, 0, 0x00, Zf, "DEC");
    check_one(ASR(16), 0x81, 0, 0xC0, C | N | S, "ASR"); // V = N xor C = 0
    check_one(ASR(16), 0x01, 0, 0x00, C | Zf | V | S, "ASR");
    check_one(LSR(16), 0x01, 0, 0x00, C | Zf | V | S, "LSR");
    check_one(LSR(16), 0x80, 0, 0x40, 0, "LSR");
    check_one(ROR(16), 0x01, C, 0x80, C | N | S, "ROR"); // V = N xor C = 0, S = N xor V = 1
    check_one(ROR(16), 0x02, 0, 0x01, 0, "ROR");
    check_one(SWAP(16), 0xA5, 0, 0x5A, 0, "SWAP");
}

TEST_CASE("avr: ADIW and SBIW take two cycles and leave H alone")
{
    Bench t({ADIW(24, 1), ADIW(26, 1), SBIW(28, 1), SBIW(30, 63)});
    t.cpu.set_reg(24, 0xFF);
    t.cpu.set_reg(25, 0xFF);
    t.cpu.set_reg(26, 0xFF);
    t.cpu.set_reg(27, 0x7F);
    t.cpu.set_reg(28, 0x00);
    t.cpu.set_reg(29, 0x80);
    t.cpu.set_reg(30, 0x00);
    t.cpu.set_reg(31, 0x00);
    t.cpu.set_sreg(H);
    CHECK(t.step() == 2);
    CHECK(t.cpu.reg(24) == 0 && t.cpu.reg(25) == 0);
    CHECK(t.flags() == (H | C | Zf));
    CHECK(t.step() == 2);
    CHECK(t.cpu.reg(26) == 0 && t.cpu.reg(27) == 0x80);
    CHECK(t.flags() == (H | V | N));
    CHECK(t.step() == 2);
    CHECK(t.cpu.reg(28) == 0xFF && t.cpu.reg(29) == 0x7F);
    CHECK(t.flags() == (H | V | S));
    CHECK(t.step() == 2);
    CHECK(t.cpu.reg(30) == 0xC1 && t.cpu.reg(31) == 0xFF);
    CHECK(t.flags() == (H | C | N | S));
}

TEST_CASE("avr: multiplications")
{
    struct Case {
        std::uint16_t insn;
        std::uint8_t a, b;
        std::uint16_t product;
        std::uint8_t flags;
        const char* what;
    };
    const Case cases[] = {
        {MUL(16, 17), 0xFF, 0xFF, 0xFE01, C, "MUL"},
        {MUL(16, 17), 0x00, 0x55, 0x0000, Zf, "MUL"},
        {MULS(16, 17), 0xFF, 0xFF, 0x0001, 0, "MULS"},      // -1 * -1
        {MULS(16, 17), 0x80, 0x7F, 0xC080, C, "MULS"},      // -128 * 127
        {MULSU(16, 17), 0xFF, 0xFF, 0xFF01, C, "MULSU"},    // -1 * 255
        {FMUL(16, 17), 0x80, 0x80, 0x8000, 0, "FMUL"},      // 1.0 * 1.0 in 1.7: 0x4000 << 1
        {FMUL(16, 17), 0xFF, 0xFF, 0xFC02, C, "FMUL"},      // 0xFE01 << 1, C from bit 15
        {FMULS(16, 17), 0x80, 0x80, 0x8000, 0, "FMULS"},    // -1 * -1: the documented overflow
        {FMULS(16, 17), 0x40, 0xC0, 0xE000, C, "FMULS"},    // 0.5 * -0.5 = -0.25
        {FMULSU(16, 17), 0xC0, 0x80, 0xC000, C, "FMULSU"},  // -0.5 * 1.0
    };
    for (const auto& c : cases) {
        Bench t({c.insn});
        t.cpu.set_reg(16, c.a);
        t.cpu.set_reg(17, c.b);
        t.cpu.set_sreg(H | V | N | S); // left alone
        const std::uint64_t cycles = t.step();
        const auto product = static_cast<std::uint16_t>(t.cpu.reg(0) | (t.cpu.reg(1) << 8));
        if (product != c.product || t.flags() != ((H | V | N | S) | c.flags) || cycles != 2) {
            char text[160];
            std::snprintf(text, sizeof text, "%s 0x%02X*0x%02X: got 0x%04X flags 0x%02X cycles %u", c.what, c.a, c.b,
                product, t.flags(), static_cast<unsigned>(cycles));
            simnodus::test::fail(__FILE__, __LINE__, text);
        }
    }
}

TEST_CASE("avr: loads and stores with their pointer updates and cycles")
{
    Bench t({LD_X(0), LD_XP(1), LD_MX(2), LD_YP(3), LD_MY(4), LD_ZP(5), LD_MZ(6), ldd(false, true, 7, 63),
        ldd(false, false, 8, 1), LDS(9), 0x0100, ST_X(10), ST_XP(10), ST_MX(10), ST_YP(10), ST_MY(10), ST_ZP(10),
        ST_MZ(10), ldd(true, true, 10, 63), ldd(true, false, 10, 2), STS(10), 0x0200, MOVW(12, 26)});
    for (int a = 0; a < 0x100; ++a) {
        t.cpu.poke(static_cast<std::uint16_t>(0x100 + a), static_cast<std::uint8_t>(a));
    }
    auto set_pair = [&](int r, int v) {
        t.cpu.set_reg(r, static_cast<std::uint8_t>(v));
        t.cpu.set_reg(r + 1, static_cast<std::uint8_t>(v >> 8));
    };
    auto pair = [&](int r) { return t.cpu.reg(r) | (t.cpu.reg(r + 1) << 8); };
    set_pair(26, 0x110);
    set_pair(28, 0x120);
    set_pair(30, 0x130);
    std::uint64_t cycles = t.step(10);
    CHECK(cycles == 20); // every load: 2 cycles
    CHECK(t.cpu.reg(0) == 0x10);
    CHECK(t.cpu.reg(1) == 0x10); // X+ reads 0x110
    CHECK(t.cpu.reg(2) == 0x10); // -X reads 0x110 again
    CHECK(pair(26) == 0x110);
    CHECK(t.cpu.reg(3) == 0x20);
    CHECK(t.cpu.reg(4) == 0x20);
    CHECK(pair(28) == 0x120);
    CHECK(t.cpu.reg(5) == 0x30);
    CHECK(t.cpu.reg(6) == 0x30);
    CHECK(pair(30) == 0x130);
    CHECK(t.cpu.reg(7) == 0x5F); // Y + 63
    CHECK(t.cpu.reg(8) == 0x31); // Z + 1
    CHECK(t.cpu.reg(9) == 0x00); // LDS 0x0100
    CHECK(t.cpu.pc() == 11);     // LDS is two words
    t.cpu.set_reg(10, 0xEE);
    cycles = t.step(10);
    CHECK(cycles == 20);
    CHECK(t.cpu.peek(0x110) == 0xEE);
    CHECK(t.cpu.peek(0x120) == 0xEE);
    CHECK(t.cpu.peek(0x130) == 0xEE);
    CHECK(t.cpu.peek(0x15F) == 0xEE);
    CHECK(t.cpu.peek(0x132) == 0xEE);
    CHECK(t.cpu.peek(0x200) == 0xEE);
    CHECK(t.cpu.peek(0x111) == 0x11); // ST X+ wrote 0x110, then -X wrote 0x110 again
    t.step();
    CHECK(t.cpu.reg(12) == 0x10 && t.cpu.reg(13) == 0x01); // MOVW: one cycle
}

TEST_CASE("avr: stack, calls and returns")
{
    // 0: RCALL to 3; 1, 2: NOP; 3: PUSH r16; 4: POP r17; 5: RET
    Bench t({RCALL(2), NOP, NOP, PUSH(16), POP(17), RET});
    t.cpu.set_reg(16, 0x42);
    CHECK(t.step() == 3);
    CHECK(t.cpu.pc() == 3);
    CHECK(t.cpu.sp() == 0x45D);
    CHECK(t.cpu.peek(0x45F) == 0x01); // return address low byte first, at the higher address
    CHECK(t.cpu.peek(0x45E) == 0x00);
    CHECK(t.step() == 2);
    CHECK(t.cpu.peek(0x45D) == 0x42); // PUSH writes at SP, then decrements it
    CHECK(t.step() == 2);
    CHECK(t.cpu.reg(17) == 0x42);
    CHECK(t.step() == 4);
    CHECK(t.cpu.pc() == 1);
    CHECK(t.cpu.sp() == 0x45F);

    // RJMP wraps around the flash (4 K words): RJMP .-2 at 0 goes to 4095.
    Bench w({RJMP(-2)});
    CHECK(w.step() == 2);
    CHECK(w.cpu.pc() == 4095);

    // IJMP, ICALL through Z.
    Bench z({ICALL, NOP, NOP, NOP, NOP, IJMP});
    z.cpu.set_reg(30, 5);
    z.cpu.set_reg(31, 0);
    CHECK(z.step() == 3);
    CHECK(z.cpu.pc() == 5);
    z.cpu.set_reg(30, 2);
    CHECK(z.step() == 2);
    CHECK(z.cpu.pc() == 2);
}

TEST_CASE("avr: branches, skips and bit instructions")
{
    Bench b({BRBS(1, 2), BRBC(1, 5)});
    b.cpu.set_sreg(Zf);
    CHECK(b.step() == 2);
    CHECK(b.cpu.pc() == 3);
    b.cpu.set_pc(1);
    CHECK(b.step() == 1); // not taken
    CHECK(b.cpu.pc() == 2);

    // Skips over a one-word and a two-word instruction.
    Bench s({CPSE(16, 17), NOP, SBRS(16, 0), LDS(0), 0x0100, SBRC(16, 0), NOP, NOP});
    s.cpu.set_reg(16, 1);
    s.cpu.set_reg(17, 1);
    CHECK(s.step() == 2);
    CHECK(s.cpu.pc() == 2);
    CHECK(s.step() == 3);
    CHECK(s.cpu.pc() == 5);
    CHECK(s.step() == 1); // bit set: no skip
    CHECK(s.cpu.pc() == 6);

    // SBI, CBI, SBIS, SBIC on I/O address 0x18 (PORTB on the ATmega8).
    Bench io({SBI(0x18, 3), SBIS(0x18, 3), NOP, CBI(0x18, 3), SBIC(0x18, 3), NOP, IN(20, 0x18), OUT(0x18, 21)});
    CHECK(io.step() == 2);
    CHECK(io.cpu.peek(0x38) == 0x08);
    CHECK(io.step() == 2);
    CHECK(io.cpu.pc() == 3);
    CHECK(io.step() == 2);
    CHECK(io.cpu.peek(0x38) == 0x00);
    CHECK(io.step() == 2);
    CHECK(io.cpu.pc() == 6);
    io.cpu.poke(0x38, 0x5A);
    io.cpu.set_reg(21, 0xC3);
    CHECK(io.step() == 1);
    CHECK(io.cpu.reg(20) == 0x5A);
    CHECK(io.step() == 1);
    CHECK(io.cpu.peek(0x38) == 0xC3);

    // BST, BLD, BSET, BCLR, and SREG and SP through IN and OUT.
    Bench f({BST(16, 7), BLD(17, 0), BSET(6), BCLR(6), IN(18, 0x3F), OUT(0x3D, 19), OUT(0x3E, 20)});
    f.cpu.set_reg(16, 0x80);
    f.cpu.set_reg(17, 0x00);
    f.step(2);
    CHECK(f.cpu.reg(17) == 0x01);
    CHECK((f.cpu.sreg() & T) != 0);
    f.step(2);
    CHECK((f.cpu.sreg() & T) == 0);
    f.cpu.set_sreg(0x15);
    f.step();
    CHECK(f.cpu.reg(18) == 0x15);
    f.cpu.set_reg(19, 0x34);
    f.cpu.set_reg(20, 0xFF);
    f.step(2);
    CHECK(f.cpu.sp() == 0x0734); // SPH has 11 bits on the ATmega8
}

TEST_CASE("avr: LPM, SPM, SLEEP, WDR and an opcode the ATmega8 lacks")
{
    Bench t({LPM, LPM_Z(16), LPM_ZP(17), LPM_ZP(18), SPM, WDR, SLEEP, 0x940C, 0x0000, NOP});
    t.cpu.set_reg(30, 0x00);
    t.cpu.set_reg(31, 0x00);
    CHECK(t.step() == 3);
    CHECK(t.cpu.reg(0) == static_cast<std::uint8_t>(0x95C8)); // low byte of word 0
    t.cpu.set_reg(30, 1);
    t.step(3);
    CHECK(t.cpu.reg(16) == 0x95);
    CHECK(t.cpu.reg(17) == 0x95);
    CHECK(t.cpu.reg(18) == static_cast<std::uint8_t>(LPM_Z(16)));
    CHECK(t.cpu.reg(30) == 3);
    int spm = 0, wdr = 0;
    t.cpu.on_spm = [&](std::uint16_t, std::uint16_t) {
        ++spm;
        return 0u;
    };
    t.cpu.on_wdr = [&] { ++wdr; };
    t.cpu.on_sleep = [] { return true; };
    t.step(3);
    CHECK(spm == 1);
    CHECK(wdr == 1);
    CHECK(t.cpu.state() == AvrState::Sleeping);
    CHECK(t.cpu.run(t.cpu.cycles() + 100) == AvrState::Sleeping); // time moves on
    t.cpu.wake();
    // JMP is not on the ATmega8: a diagnostic, run as a two-word NOP.
    CHECK(t.step() == 1);
    CHECK(t.cpu.pc() == 9);
    CHECK(t.diagnostics.size() == 1);
}

TEST_CASE("avr: interrupts take four cycles, after SEI and RETI one more instruction runs")
{
    // 0: reset; 1: vector 1 -> RETI; 2..: main
    Bench t({RJMP(1), RETI, BSET(7), NOP, NOP, NOP});
    t.cpu.set_pc(2);
    int taken = 0;
    t.cpu.pending_vector = [] { return 1; };
    t.cpu.interrupt_taken = [&](int vector) {
        taken = vector;
        t.cpu.set_interrupt_pending(false);
    };
    t.cpu.set_interrupt_pending(true);
    CHECK(t.step() == 1); // SEI
    CHECK(t.step() == 1); // the NOP after SEI runs first
    CHECK(t.cpu.pc() == 4);
    CHECK(t.step() == 4); // then the interrupt
    CHECK(taken == 1);
    CHECK(t.cpu.pc() == 1);
    CHECK((t.cpu.sreg() & I) == 0);
    CHECK(t.cpu.peek(0x45F) == 4); // the PC to return to
    t.cpu.set_interrupt_pending(true);
    CHECK(t.step() == 4); // RETI with an interrupt still pending
    CHECK(t.cpu.pc() == 4);
    CHECK(t.step() == 1); // one main instruction runs
    CHECK(t.cpu.pc() == 5);
    CHECK(t.step() == 4); // then the interrupt again
    CHECK(t.cpu.pc() == 1);

    // From sleep the response takes four cycles more.
    Bench s({RJMP(1), RETI, SLEEP, NOP});
    s.cpu.set_pc(2);
    s.cpu.set_sreg(I);
    s.cpu.on_sleep = [] { return true; };
    s.cpu.pending_vector = [] { return 1; };
    s.cpu.interrupt_taken = [&](int) { s.cpu.set_interrupt_pending(false); };
    s.step();
    CHECK(s.cpu.state() == AvrState::Sleeping);
    s.cpu.set_interrupt_pending(true);
    CHECK(s.step() == 8);
    CHECK(s.cpu.state() == AvrState::Running);
    CHECK(s.cpu.pc() == 1);
}

// ---------------------------------------------------------------------------
// Gate G10a.4: robustness.

TEST_CASE("avr: every opcode decodes, random images run without a crash")
{
    AvrCpu cpu;
    int undefined = 0;
    for (unsigned w = 0; w < 0x10000; ++w) {
        const AvrInsn in = cpu.decode(static_cast<std::uint16_t>(w), 0x1234);
        CHECK(in.op != 0);
        CHECK(in.size == 1 || in.size == 2);
        undefined += std::string(AvrCpu::op_name(in.op)) == "undefined" ? 1 : 0;
    }
    std::printf("  undefined opcodes on the ATmega8: %d of 65536\n", undefined);
    std::mt19937 random(10);
    for (int image = 0; image < 200; ++image) {
        Bench t({});
        std::vector<std::uint16_t> words(4096);
        for (auto& w : words) {
            w = static_cast<std::uint16_t>(random());
        }
        t.cpu.load_flash(words);
        t.cpu.diagnostic = nullptr;
        t.cpu.on_sleep = [] { return false; };
        t.cpu.run(100'000);
        CHECK(t.cpu.cycles() >= 100'000);
    }
}

// ---------------------------------------------------------------------------
// Gate G10a.3: speed (MIPS on the computer that runs the test; the gate is
// measured on the owner's laptop with a Release build).

namespace {

double mips(const std::vector<std::uint16_t>& program, double& real_time)
{
    AvrCpu cpu;
    cpu.load_flash(program);
    cpu.set_sp(0x45F);
    for (int r : {26, 28, 30}) {
        cpu.set_reg(r, 0x00);
        cpu.set_reg(r + 1, 0x01);
    }
    const auto start = std::chrono::steady_clock::now();
    double seconds = 0;
    while (seconds < 0.5) {
        cpu.run(cpu.cycles() + 1'000'000);
        seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    }
    real_time = static_cast<double>(cpu.cycles()) / seconds / 16e6;
    return static_cast<double>(cpu.instructions()) / seconds / 1e6;
}

} // namespace

TEST_CASE("avr-bench: busy loop and instruction mix")
{
    // loop: SUBI r24,1; SBCI r25,0; BRNE loop; RJMP loop
    const std::vector<std::uint16_t> busy = {SUBI(24, 1), SBCI(25, 0), BRBC(1, -3), RJMP(-4)};
    // loop: loads, a multiplication, 16-bit sums, a store, pointers kept in
    // 0x100-0x17F, a 16-bit counter and a branch.
    const std::vector<std::uint16_t> mix = {LD_XP(16), ldd(false, true, 17, 3), MUL(16, 17), ADD(2, 0), ADC(3, 1),
        ST_ZP(16), ANDI(26, 0x7F), ANDI(30, 0x7F), ADIW(24, 1), CPI(24, 0), BRBC(1, -11), RJMP(-12)};
    double busy_rt = 0;
    double mix_rt = 0;
    const double busy_mips = mips(busy, busy_rt);
    const double mix_mips = mips(mix, mix_rt);
    std::printf("  busy loop: %.1f MIPS (%.1fx real time at 16 MHz)\n", busy_mips, busy_rt);
    std::printf("  instruction mix: %.1f MIPS (%.1fx real time at 16 MHz)\n", mix_mips, mix_rt);
    CHECK(busy_mips > 0);
    CHECK(mix_mips > 0);
}

// ---------------------------------------------------------------------------
// Gate G10a.2: random blocks against avr8js's recorded results
// (tests/experiments/avr-differential).

namespace {

// Record layout shared with tests/experiments/avr-differential/run_avr8js.js.
constexpr std::size_t block_words = 128;
constexpr std::size_t ram_base = 0x100;
constexpr std::size_t ram_bytes = 128;
constexpr std::size_t input_size = 32 + 1 + 2 + ram_bytes + 2 * block_words;     // regs, SREG, SP, RAM, code
constexpr std::size_t output_size = 32 + 1 + 2 + 2 + 4 + 4 + ram_bytes;           // regs, SREG, SP, PC, cycles, steps, RAM
constexpr int max_steps = 400;
constexpr std::uint16_t stack_floor = 0x68;

} // namespace

TEST_CASE("avr-diff: differential corpus against avr8js")
{
    const fs::path dir = repository / "tests" / "experiments" / "avr-differential";
    const std::vector<std::uint8_t> inputs = read_file(dir / "blocks.bin");
    const std::vector<std::uint8_t> outputs = read_file(dir / "avr8js.bin");
    REQUIRE(!inputs.empty() && inputs.size() % input_size == 0);
    const std::size_t blocks = inputs.size() / input_size;
    REQUIRE(outputs.size() == blocks * output_size);
    std::vector<std::uint64_t> hits(static_cast<std::size_t>(AvrCpu::op_count()), 0);
    std::size_t differing = 0;
    for (std::size_t b = 0; b < blocks; ++b) {
        const std::uint8_t* in = inputs.data() + b * input_size;
        AvrConfig config;
        AvrCpu cpu(config);
        cpu.count_ops(true);
        std::vector<std::uint16_t> code(block_words);
        const std::uint8_t* code_bytes = in + 32 + 1 + 2 + ram_bytes;
        for (std::size_t i = 0; i < block_words; ++i) {
            code[i] = static_cast<std::uint16_t>(code_bytes[2 * i] | (code_bytes[2 * i + 1] << 8));
        }
        cpu.load_flash(code);
        for (int r = 0; r < 32; ++r) {
            cpu.set_reg(r, in[r]);
        }
        cpu.set_sreg(in[32]);
        cpu.set_sp(static_cast<std::uint16_t>(in[33] | (in[34] << 8)));
        for (std::size_t a = 0; a < ram_bytes; ++a) {
            cpu.poke(static_cast<std::uint16_t>(ram_base + a), in[35 + a]);
        }
        cpu.on_sleep = [] { return false; };
        int steps = 0;
        const bool tracing = trace_block >= 0 && static_cast<std::size_t>(trace_block) == b;
        // A run also ends when the stack reaches the I/O registers, as in
        // run_avr8js.js.
        while (steps < max_steps && cpu.pc() < block_words - 1 && cpu.sp() >= stack_floor) {
            if (tracing) {
                std::printf("%3d pc=%04X op=%04X cyc=%llu sreg=%02X sp=%04X r24=%02X%02X\n", steps,
                    static_cast<unsigned>(cpu.pc()), cpu.flash_word(cpu.pc()), static_cast<unsigned long long>(cpu.cycles()),
                    cpu.sreg(), cpu.sp(), cpu.reg(25), cpu.reg(24));
            }
            cpu.step_instruction();
            ++steps;
        }
        std::vector<std::uint8_t> got(output_size);
        for (int r = 0; r < 32; ++r) {
            got[static_cast<std::size_t>(r)] = cpu.reg(r);
        }
        got[32] = cpu.sreg();
        got[33] = static_cast<std::uint8_t>(cpu.sp());
        got[34] = static_cast<std::uint8_t>(cpu.sp() >> 8);
        got[35] = static_cast<std::uint8_t>(cpu.pc());
        got[36] = static_cast<std::uint8_t>(cpu.pc() >> 8);
        for (int i = 0; i < 4; ++i) {
            got[37 + static_cast<std::size_t>(i)] = static_cast<std::uint8_t>(cpu.cycles() >> (8 * i));
            got[41 + static_cast<std::size_t>(i)] = static_cast<std::uint8_t>(static_cast<unsigned>(steps) >> (8 * i));
        }
        for (std::size_t a = 0; a < ram_bytes; ++a) {
            got[45 + a] = cpu.peek(static_cast<std::uint16_t>(ram_base + a));
        }
        const std::uint8_t* expected = outputs.data() + b * output_size;
        if (std::memcmp(got.data(), expected, output_size) != 0) {
            if (differing++ < 5) {
                std::size_t at = 0;
                while (got[at] == expected[at]) {
                    ++at;
                }
                char text[160];
                std::snprintf(text, sizeof text, "block %zu: first difference at byte %zu (got 0x%02X, avr8js 0x%02X)", b, at,
                    got[at], expected[at]);
                simnodus::test::fail(__FILE__, __LINE__, text);
            }
        }
        const auto& counted = cpu.op_hits();
        for (std::size_t i = 0; i < hits.size(); ++i) {
            hits[i] += counted[i];
        }
    }
    std::printf("  %zu blocks, %zu differing\n", blocks, differing);
    std::string low;
    for (std::size_t i = 2; i < hits.size(); ++i) {
        if (hits[i] < 1000) {
            low += std::string(" ") + AvrCpu::op_name(static_cast<std::uint8_t>(i)) + "=" + std::to_string(hits[i]);
        }
    }
    std::printf("  operations below 1000 hits:%s\n", low.empty() ? " none" : low.c_str());
}

// ---------------------------------------------------------------------------
// The ATmega8 machine (SN-257, gate G10b): the owned firmware set in
// examples/firmware/avr_*, and small programs for what it does not cover.

namespace {

constexpr int pb0 = 0, pb1 = 1, pb2 = 2, pb3 = 3, pc6 = 14, pd0 = 16, pd1 = 17, pd2 = 18, pd7 = 23;

std::unique_ptr<Atmega> machine_with(const std::string& name, Atmega::Options options = {})
{
    auto m = std::make_unique<Atmega>(options);
    std::string error;
    const std::string folder = name.substr(0, name.rfind("_16mhz") == std::string::npos ? name.size() : name.rfind("_16mhz"));
    const auto image = read_elf(repository / "examples" / "firmware" / folder / (name + ".elf"), error, ElfMachine::Avr);
    if (!image || !m->load_elf(*image, error)) {
        simnodus::test::fail(__FILE__, __LINE__, error);
        return nullptr;
    }
    m->reset();
    return m;
}

std::unique_ptr<Atmega> machine_with_words(const std::vector<std::uint16_t>& words, Atmega::Options options = {})
{
    auto m = std::make_unique<Atmega>(options);
    std::vector<std::uint8_t> bytes;
    for (const std::uint16_t w : words) {
        bytes.push_back(static_cast<std::uint8_t>(w));
        bytes.push_back(static_cast<std::uint8_t>(w >> 8));
    }
    std::string error;
    m->load_flash(bytes, error);
    m->reset();
    return m;
}

// Times (ns) at which `pin` changes its driven level.
std::vector<std::uint64_t> edges(const std::vector<Atmega::PinEvent>& events, int pin)
{
    std::vector<std::uint64_t> times;
    bool have = false;
    bool level = false;
    for (const auto& e : events) {
        if (e.pin != pin || !e.state.output) {
            continue;
        }
        if (have && e.state.level != level) {
            times.push_back(e.time_ns);
        }
        have = true;
        level = e.state.level;
    }
    return times;
}

std::string uart_text(Atmega& m)
{
    std::string text;
    for (const auto& b : m.take_uart_bytes()) {
        text += static_cast<char>(b.value);
    }
    return text;
}

} // namespace

TEST_CASE("atmega8: blink with _delay_ms at the factory 1 MHz and with a 16 MHz crystal")
{
    auto m = machine_with("avr_blink");
    REQUIRE(m);
    CHECK(m->clock_hz() == 1'000'000);
    m->run_until_ns(3'000'000'000);
    const auto t = edges(m->take_pin_events(), pb0);
    REQUIRE(t.size() >= 5);
    // The factory fuses (SUT 10) give 6 clocks and 65 ms before the first
    // instruction; the first toggle comes right after.
    CHECK(t[0] > 65'000'000 && t[0] < 65'200'000);
    // _delay_ms(500) is 500 000 cycles; the loop adds the same few each time.
    const std::uint64_t period = t[2] - t[1];
    CHECK(period >= 500'000'000 && period < 500'010'000);
    for (std::size_t i = 2; i < t.size(); ++i) {
        CHECK(t[i] - t[i - 1] == period);
    }

    // The 16 MHz build carries its fuses (crystal, CKOPT) in the ELF.
    Atmega::Options crystal;
    crystal.external_hz = 16'000'000;
    auto fast = machine_with("avr_blink_16mhz", crystal);
    REQUIRE(fast);
    CHECK(fast->fuse_low() == 0xEF);
    CHECK(fast->fuse_high() == 0xC9);
    CHECK(fast->clock_hz() == 16'000'000);
    fast->run_until_ns(2'000'000'000);
    const auto f = edges(fast->take_pin_events(), pb0);
    REQUIRE(f.size() >= 3);
    const std::uint64_t fast_period = f[2] - f[1];
    CHECK(fast_period >= 500'000'000 && fast_period < 500'001'000); // 8 000 000 cycles and the loop's few
    // 16K clocks and 4.1 ms of start-up (CKSEL 1111, SUT 10).
    CHECK(f[0] > 5'100'000 && f[0] < 5'200'000);

    // The same fuses without a crystal on XTAL1/XTAL2: the chip does not start.
    auto none = machine_with("avr_blink_16mhz");
    REQUIRE(none);
    CHECK(!none->running());
    none->run_until_ns(1'000'000'000);
    CHECK(edges(none->take_pin_events(), pb0).empty());
    CHECK(none->time_ns() == 1'000'000'000);
    const auto notes = none->take_diagnostics();
    REQUIRE(!notes.empty());
    CHECK(notes[0].find("does not start") != std::string::npos);
}

TEST_CASE("atmega8: Timer1 fast PWM, Timer2 phase-correct PWM and Timer0 overflow, exact in cycles")
{
    auto m = machine_with("avr_pwm");
    REQUIRE(m);
    m->run_until_ns(400'000'000);
    const auto events = m->take_pin_events();
    // Timer1 mode 14, ICR1 = 1999: 2000 cycles (2 ms) a period; OC1A high
    // for OCR1A + 1 = 500 cycles; OC1B inverting, high for 1999 - 1499 = 500.
    for (const int pin : {pb1, pb2}) {
        const auto t = edges(events, pin);
        REQUIRE(t.size() > 20);
        for (std::size_t i = 4; i + 2 < t.size(); i += 2) {
            CHECK(t[i + 2] - t[i] == 2'000'000);
        }
        // The two parts of a period: 500 cycles high, 1500 low.
        const std::uint64_t a = t[5] - t[4];
        const std::uint64_t b = t[6] - t[5];
        CHECK((a == 500'000 && b == 1'500'000) || (a == 1'500'000 && b == 500'000));
    }
    // Timer2 phase correct, prescaler 8: 8 * 510 = 4080 cycles a period,
    // high for 8 * 2 * 64 = 1024.
    const auto t2 = edges(events, pb3);
    REQUIRE(t2.size() > 20);
    for (std::size_t i = 4; i + 2 < t2.size(); i += 2) {
        CHECK(t2[i + 2] - t2[i] == 4'080'000);
    }
    const std::uint64_t a = t2[5] - t2[4];
    CHECK(a == 1'024'000 || a == 4'080'000 - 1'024'000);
    // Timer0 overflow every 64 * 256 = 16 384 cycles; the interrupt's
    // latency is the same each time.
    const auto t0 = edges(events, pd7);
    REQUIRE(t0.size() > 10);
    for (std::size_t i = 2; i < t0.size(); ++i) {
        CHECK(t0[i] - t0[i - 1] == 16'384'000);
    }
}

TEST_CASE("atmega8: USART at 9600 baud: bits on TXD, echo, EEPROM kept over a reset")
{
    auto m = machine_with("avr_serial");
    REQUIRE(m);
    m->run_until_ns(200'000'000);
    CHECK(uart_text(*m) == "ATmega8 ready\r\n> ");
    // Each bit lasts 8 * (12 + 1) = 104 cycles with U2X (9615 baud).
    const auto t = edges(m->take_pin_events(), pd1);
    REQUIRE(t.size() > 10);
    for (std::size_t i = 1; i < t.size(); ++i) {
        const std::uint64_t d = t[i] - t[i - 1];
        if (d < 2'000'000) { // inside a frame
            CHECK(d % 104'000 == 0);
        }
    }
    for (const char c : std::string("ab?")) {
        m->uart_receive(static_cast<std::uint8_t>(c));
    }
    m->run_until_ns(400'000'000);
    CHECK(uart_text(*m) == "ab\r\nbytes: 3\r\n> ");
    // The counter lives in the EEPROM: it continues after a reset.
    CHECK(m->eeprom_writes() > 0);
    m->reset(Atmega::ResetKind::External);
    m->run_until_ns(m->time_ns() + 200'000'000);
    CHECK(uart_text(*m) == "ATmega8 ready\r\n> ");
    m->uart_receive('?');
    m->run_until_ns(m->time_ns() + 100'000'000);
    CHECK(uart_text(*m) == "\r\nbytes: 4\r\n> ");
    CHECK((m->reset_flags() & 0x02) != 0); // EXTRF

    // Frames on RXD from the circuit, bit by bit.
    auto p = machine_with("avr_serial");
    REQUIRE(p);
    p->run_until_ns(200'000'000);
    uart_text(*p);
    p->set_pin_input(pd0, true); // the other side holds the line high while idle
    std::uint64_t at = p->time_ns() + 1'000'000;
    auto drive = [&](bool level) {
        p->run_until_ns(at);
        p->set_pin_input(pd0, level);
        at += 104'000; // one bit at 1 MHz
    };
    const std::uint8_t byte = 'Z';
    drive(false); // start bit
    for (int b = 0; b < 8; ++b) {
        drive(((byte >> b) & 1) != 0);
    }
    drive(true); // stop bit
    p->run_until_ns(at + 20'000'000);
    CHECK(uart_text(*p) == "Z");
}

TEST_CASE("atmega8: INT0 on a falling edge with a debounce from a Timer1 CTC tick")
{
    auto m = machine_with("avr_button");
    REQUIRE(m);
    m->run_until_ns(100'000'000);
    m->take_pin_events();
    auto led = [&] { return m->pin(pb0).level; };
    const bool before = led();
    m->set_pin_input(pd2, false); // pressed
    m->run_until_ns(101'000'000);
    CHECK(led() != before);
    m->set_pin_input(pd2, true); // a bounce 2 ms later: ignored
    m->run_until_ns(103'000'000);
    m->set_pin_input(pd2, false);
    m->run_until_ns(104'000'000);
    CHECK(led() != before);
    m->set_pin_input(pd2, true);
    m->run_until_ns(200'000'000);
    m->set_pin_input(pd2, false); // a second press, 96 ms later
    m->run_until_ns(201'000'000);
    CHECK(led() == before);
    // Two presses: PD4..PD7 show 2.
    CHECK(!m->pin(20).level && m->pin(21).level && !m->pin(22).level && !m->pin(pd7).level);
}

TEST_CASE("atmega8: watchdog reset, WDTON, RSTDISBL, the RESET pin and reading the fuses")
{
    // LDI r16, WDE; OUT WDTCR, r16; RJMP .-1: a reset after 16K cycles of the
    // 1 MHz watchdog oscillator (16.4 ms).
    auto w = machine_with_words({LDI(16, 0x08), OUT(0x21, 16), RJMP(-1)});
    w->run_until_ns(100'000'000);
    CHECK((w->reset_flags() & 0x08) != 0); // WDRF
    // WDTON programmed: the watchdog runs without the firmware.
    Atmega::Options wdton;
    wdton.fuse_high = 0xD9 & ~0x40;
    auto v = machine_with_words({RJMP(-1)}, wdton);
    v->run_until_ns(100'000'000);
    CHECK((v->reset_flags() & 0x08) != 0);
    // Without WDTON nothing resets it.
    auto quiet = machine_with_words({RJMP(-1)});
    quiet->run_until_ns(100'000'000);
    CHECK(quiet->reset_flags() == 0x01); // PORF only

    // PC6: RESET by default (the pin resets the chip), an I/O pin with RSTDISBL.
    auto r = machine_with_words({RJMP(-1)});
    r->run_until_ns(80'000'000);
    r->set_pin_input(pc6, false);
    r->run_until_ns(81'000'000);
    r->set_pin_input(pc6, true);
    r->run_until_ns(200'000'000);
    CHECK((r->reset_flags() & 0x02) != 0); // EXTRF
    Atmega::Options io;
    io.fuse_high = 0xD9 & ~0x80;
    // SBI DDRC, 6; SBI PORTC, 6; RJMP .-1
    auto o = machine_with_words({SBI(0x14, 6), SBI(0x15, 6), RJMP(-1)}, io);
    o->run_until_ns(80'000'000);
    CHECK(o->pin(pc6).output);
    CHECK(o->pin(pc6).level);

    // The fuse and lock bytes through LPM after BLBSET and SPMEN.
    // LDI r30,0; LDI r31,0; LDI r16,0x09; OUT SPMCR,r16; LPM; STS 0x100,r0;
    // LDI r30,3; OUT SPMCR,r16; LPM; STS 0x101,r0; RJMP .-1
    auto f = machine_with_words({LDI(30, 0), LDI(31, 0), LDI(16, 0x09), OUT(0x37, 16), LPM, STS(0), 0x0100, LDI(30, 3),
        OUT(0x37, 16), LPM, STS(0), 0x0101, RJMP(-1)});
    f->run_until_ns(80'000'000);
    CHECK(f->cpu().peek(0x100) == 0xE1);
    CHECK(f->cpu().peek(0x101) == 0xD9);
}

TEST_CASE("atmega8: ADC codes, references, timing and free running; the analog comparator (SN-259)")
{
    // Voltages from "the circuit": PC0 2.5 V, PC1 1.0 V, PD6 2.0 V, PD7 set below.
    double ain1 = 1.0;
    auto m = machine_with("avr_adc");
    REQUIRE(m);
    m->set_analog_source([&](int pin, std::uint64_t) -> std::optional<double> {
        switch (pin) {
        case 8: return 2.5;
        case 9: return 1.0;
        case 22: return 2.0;
        case 23: return ain1;
        default: return std::nullopt;
        }
    });
    m->run_until_ns(300'000'000);
    // 2.5 V of 5 V: 512; 1.0 V: 204; 2.5 V of 2.56 V: 1000; the bandgap: 266.
    const std::string text = uart_text(*m);
    CHECK(text.find("A0 512 A1 204 A0/2.56 1000 BG 266\r\n") != std::string::npos);

    // Timing through the registers: the first conversion after ADEN takes
    // 25 ADC clocks, the next 13 (prescaler 8: 200 and 104 cycles).
    auto t = machine_with_words({RJMP(-1)});
    t->set_analog_source([&](int pin, std::uint64_t) -> std::optional<double> {
        if (pin >= Atmega::vcc_pin) {
            return std::nullopt; // VCC, AVCC, AREF: 5 V
        }
        return pin == 22 ? 2.0 : pin == 23 ? ain1 : 1.25; // AIN0, AIN1, the ADC inputs
    });
    t->run_until_ns(70'000'000);
    auto& cpu = t->cpu();
    auto busy = [&] { return (cpu.read_data(0x26) & 0x40) != 0; };
    auto run_cycles = [&](std::uint64_t n) { t->run_until_ns(t->ns_at(t->cycles() + n)); };
    cpu.write_data(0x27, 0x40);       // AVCC, ADC0
    cpu.write_data(0x26, 0x80 | 0x40 | 0x03); // ADEN, ADSC, /8
    // The core runs a two-cycle RJMP: the registers are read between
    // instructions, so the checks stand 10 cycles either side of the end.
    run_cycles(190);
    CHECK(busy());
    run_cycles(20);
    CHECK(!busy());
    CHECK((cpu.read_data(0x26) & 0x10) != 0); // ADIF
    CHECK((cpu.read_data(0x24) | (cpu.read_data(0x25) << 8)) == 256); // 1.25 V of 5 V
    cpu.write_data(0x26, 0x80 | 0x40 | 0x10 | 0x03); // a new one, and ADIF cleared
    CHECK((cpu.read_data(0x26) & 0x10) == 0);
    run_cycles(94);
    CHECK(busy());
    run_cycles(20);
    CHECK(!busy());
    // ADLAR: left adjusted, ADCH holds the eight high bits.
    cpu.write_data(0x27, 0x60);
    cpu.write_data(0x26, 0x80 | 0x40 | 0x03);
    run_cycles(110);
    CHECK(cpu.read_data(0x25) == 256 >> 2);
    // Free running: a conversion every 13 ADC clocks without ADSC writes.
    cpu.write_data(0x27, 0x40);
    cpu.write_data(0x26, 0x80 | 0x40 | 0x20 | 0x10 | 0x03);
    int ends = 0;
    const std::uint64_t from = t->cycles();
    while (t->cycles() - from < 1040) {
        const bool before = (cpu.read_data(0x26) & 0x10) != 0;
        run_cycles(1);
        if (!before && (cpu.read_data(0x26) & 0x10) != 0) {
            ++ends;
            cpu.write_data(0x26, 0x80 | 0x40 | 0x20 | 0x10 | 0x03); // clear ADIF
        }
    }
    CHECK(ends == 10); // 1040 cycles / 104

    // The comparator: AIN0 (2.0 V) against AIN1; ACI on a rising output.
    cpu.write_data(0x26, 0x00); // ADC off
    cpu.write_data(0x28, 0x13); // ACI cleared, ACIS 11
    run_cycles(100);
    CHECK((cpu.read_data(0x28) & 0x20) != 0); // ACO: 2.0 V > 1.0 V
    CHECK((cpu.read_data(0x28) & 0x10) == 0); // no edge yet
    ain1 = 3.0;
    run_cycles(20'000); // sampled every 10 us
    CHECK((cpu.read_data(0x28) & 0x20) == 0);
    CHECK((cpu.read_data(0x28) & 0x10) == 0); // falling: not the rising mode
    ain1 = 1.0;
    run_cycles(20'000);
    CHECK((cpu.read_data(0x28) & 0x10) != 0); // rising: ACI
    // The bandgap (1.30 V) as the positive input.
    cpu.write_data(0x28, 0x50);
    ain1 = 1.5;
    run_cycles(20'000);
    CHECK((cpu.read_data(0x28) & 0x20) == 0);
}

TEST_CASE("atmega8: TWI master against a slave at the pin level (SN-259)")
{
    auto m = machine_with("avr_bus");
    REQUIRE(m);
    CHECK(m->clock_hz() == 8'000'000); // FUSES in the ELF: low E4
    // An I2C slave at 0x48 that answers with 0x19, 0x80 (25.5 C, as an LM75)
    // and ACKs everything; the 4.7 kOhm pull-ups are the default high level.
    constexpr int sda = 12, scl = 13;
    struct Slave {
        bool sda_low = false;
        bool last_scl = true, last_sda = true;
        int bit = 0;
        bool active = false, reading = false, address = false, ack_phase = false;
        unsigned shift = 0;
        int sent = 0;
        int starts = 0;
    } s;
    const std::uint8_t reply[2] = {0x19, 0x80};
    auto levels = [&](bool& scl_level, bool& sda_level) {
        const auto c = m->pin(scl);
        const auto d = m->pin(sda);
        scl_level = !(c.output && !c.level);
        sda_level = !(d.output && !d.level) && !s.sda_low;
    };
    bool busy = false;
    std::function<void()> settle = [&] {
        if (busy) {
            return;
        }
        busy = true;
        bool c = true, d = true;
        levels(c, d);
        if (c && s.last_scl && s.last_sda && !d) { // START
            s.active = true;
            s.address = true;
            s.bit = 0;
            s.shift = 0;
            s.ack_phase = false;
            ++s.starts;
        } else if (c && s.last_scl && !s.last_sda && d) { // STOP
            s.active = false;
            s.sda_low = false;
        } else if (s.active && c && !s.last_scl) { // SCL rises: sample
            if (!s.ack_phase && !(s.reading && !s.address)) {
                s.shift = (s.shift << 1) | (d ? 1u : 0u);
            }
            ++s.bit;
        } else if (s.active && !c && s.last_scl) { // SCL falls: drive
            if (s.bit == 8 && !s.ack_phase) {
                s.ack_phase = true;
                if (s.address) {
                    s.reading = (s.shift & 1u) != 0;
                    s.sda_low = (s.shift >> 1) == 0x48; // ACK our address
                } else {
                    s.sda_low = !s.reading; // ACK written data
                }
            } else if (s.bit == 9) {
                s.ack_phase = false;
                s.address = false;
                s.bit = 0;
                s.shift = 0;
                s.sda_low = false;
                if (s.reading) {
                    s.sda_low = ((reply[s.sent % 2] >> 7) & 1u) == 0;
                }
            } else if (s.reading && !s.address && !s.ack_phase) {
                s.sda_low = ((reply[s.sent % 2] >> (7 - s.bit)) & 1u) == 0;
                if (s.bit == 7) {
                    // after the last bit the master ACKs or NACKs
                }
            }
            if (s.reading && !s.address && s.bit == 8 && s.ack_phase) {
                s.sda_low = false; // the master drives the ACK
                ++s.sent;
            }
        }
        levels(c, d);
        s.last_scl = c;
        s.last_sda = d;
        m->set_pin_input(scl, c);
        m->set_pin_input(sda, d);
        busy = false;
    };
    m->set_pin_hook([&](int pin, std::uint64_t) {
        if (pin == sda || pin == scl) {
            settle();
        }
    });
    m->set_pin_input(sda, true);
    m->set_pin_input(scl, true);
    m->run_until_ns(800'000'000);
    const std::string text = uart_text(*m);
    std::printf("  %s", text.c_str());
    CHECK(s.starts > 0);
    CHECK(text.find("LM75 25.5 C") != std::string::npos);
}

TEST_CASE("atmega8: Timer2 on a 32.768 kHz crystal wakes the chip from power-save every second (SN-259)")
{
    Atmega::Options options;
    options.external_hz = 32768; // the watch crystal between TOSC1 and TOSC2
    auto m = machine_with("avr_sleep", options);
    REQUIRE(m);
    m->run_until_ns(4'500'000'000);
    const auto t = edges(m->take_pin_events(), pb0);
    REQUIRE(t.size() >= 4);
    // 128 x 256 ticks of 32768 Hz: exactly 1 s, and the same interrupt latency each time.
    for (std::size_t i = 1; i < t.size(); ++i) {
        CHECK(t[i] - t[i - 1] == 1'000'000'000);
    }
    CHECK(uart_text(*m).find("seconds 4\r\n") != std::string::npos);
    // Asleep between the seconds: a few thousand instructions in 4.5 s.
    CHECK(m->cpu().instructions() < 40'000);
    CHECK(!m->pin_usable(6)); // PB6, PB7: TOSC1, TOSC2
}

TEST_CASE("atmega8: power-down wakes on a level interrupt, not on an edge; the timers stop (SN-259)")
{
    // 0: RJMP main; 1 (INT0): RETI; main at 2:
    //   LDI r16,0x40; OUT GICR,r16; LDI r16,<MCUCR>; OUT MCUCR,r16; SEI; SLEEP;
    //   SBI DDRB,0; SBI PORTB,0; RJMP .-1
    auto program = [](int mcucr) {
        return std::vector<std::uint16_t>{RJMP(1), RETI, LDI(16, 0x40), OUT(0x3B, 16), LDI(16, mcucr), OUT(0x35, 16),
            BSET(7), SLEEP, SBI(0x17, 0), SBI(0x18, 0), RJMP(-1)};
    };
    for (const bool level : {true, false}) {
        // SE, SM 010 (power-down); ISC0 00 (low level) or 10 (falling edge).
        auto m = machine_with_words(program(level ? 0xA0 : 0xA2));
        m->set_pin_input(pd2, true);
        m->run_until_ns(100'000'000);
        CHECK(!m->pin(pb0).output); // asleep
        const auto asleep = m->cpu().instructions();
        m->set_pin_input(pd2, false);
        m->run_until_ns(101'000'000);
        CHECK(m->pin(pb0).output == level);
        if (!level) {
            CHECK(m->cpu().instructions() == asleep); // an edge does not wake power-down
        }
    }
    // Timer1 stands still in power-down: a CTC interrupt never comes, though
    // Idle would take it. 0: RJMP main; 6 (TIMER1 COMPA): SBI PORTB,1; RETI.
    auto timer = [](int mcucr) {
        return std::vector<std::uint16_t>{RJMP(7), NOP, NOP, NOP, NOP, NOP, SBI(0x18, 1), RETI, SBI(0x17, 1),
            LDI(16, 0x03), OUT(0x2B, 16), LDI(16, 0xE8), OUT(0x2A, 16), LDI(16, 0x10), OUT(0x39, 16), LDI(16, 0x09),
            OUT(0x2E, 16), LDI(16, mcucr), OUT(0x35, 16), BSET(7), SLEEP, RJMP(-2)};
    };
    // OCR1AH = 0x03, OCR1AL = 0xE8 (1000 cycles), OCIE1A, CTC with CK/1.
    auto idle = machine_with_words(timer(0x80));
    idle->run_until_ns(100'000'000);
    CHECK(idle->pin(pb1).level); // Idle: the timer runs and its interrupt wakes the chip
    auto down = machine_with_words(timer(0xA0));
    down->run_until_ns(100'000'000);
    CHECK(!down->pin(pb1).level);
}

TEST_CASE("atmega8: VCC from the circuit: power-on, brown-out, a low supply, the ADC's AVCC (SN-259)")
{
    double vcc = 5.0;
    auto m = machine_with("avr_blink");
    REQUIRE(m);
    m->set_fuses(0xE1 & ~0x40, 0xD9, 0xFF); // BODEN programmed, BODLEVEL 2.6 V (the ELF had the factory fuses)
    m->reset();
    m->set_analog_source([&](int pin, std::uint64_t) -> std::optional<double> {
        return pin == Atmega::vcc_pin ? std::optional<double>(vcc) : std::nullopt;
    });
    m->run_until_ns(1'200'000'000);
    CHECK(edges(m->take_pin_events(), pb0).size() >= 2);
    vcc = 2.3; // below the brown-out level: held in reset
    m->run_until_ns(2'500'000'000);
    CHECK(edges(m->take_pin_events(), pb0).empty());
    CHECK(!m->pin(pb0).output);
    vcc = 5.0;
    m->run_until_ns(4'000'000'000);
    CHECK((m->reset_flags() & 0x04) != 0); // BORF
    CHECK(edges(m->take_pin_events(), pb0).size() >= 2);
    // No power at all: off, with a message, and a power-on reset afterwards.
    vcc = 0.0;
    m->run_until_ns(4'500'000'000);
    bool told = false;
    for (const auto& d : m->take_diagnostics()) {
        told = told || d.find("has no power") != std::string::npos;
    }
    CHECK(told);
    vcc = 5.0;
    m->run_until_ns(5'000'000'000);
    CHECK(m->reset_flags() == 0x01); // PORF only

    // An ATmega8 at 16 MHz on 4.0 V without brown-out detection: a warning.
    Atmega::Options fast;
    fast.model = Atmega::Model::Atmega8;
    fast.external_hz = 16'000'000;
    auto f = machine_with("avr_blink_16mhz", fast);
    REQUIRE(f);
    f->set_analog_source([](int pin, std::uint64_t) -> std::optional<double> {
        return pin == Atmega::vcc_pin ? std::optional<double>(4.0) : std::nullopt;
    });
    f->run_until_ns(50'000'000);
    bool warned = false;
    for (const auto& d : f->take_diagnostics()) {
        warned = warned || d.find("below the 4.5 V") != std::string::npos;
    }
    CHECK(warned);

    // The ADC's AVCC reference is the circuit's: 2.0 V of 4.0 V is 512.
    auto a = machine_with("avr_adc");
    REQUIRE(a);
    a->set_analog_source([](int pin, std::uint64_t) -> std::optional<double> {
        if (pin == Atmega::avcc_pin) {
            return 4.0;
        }
        return pin == 8 ? std::optional<double>(2.0) : std::nullopt;
    });
    a->run_until_ns(300'000'000);
    CHECK(uart_text(*a).find("A0 512 ") != std::string::npos);
}

TEST_CASE("atmega8: Intel HEX loading and a chip erase that keeps the EEPROM with EESAVE")
{
    Atmega m;
    std::string error;
    std::ifstream in(repository / "examples" / "firmware" / "avr_blink" / "avr_blink.hex");
    const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    REQUIRE(!text.empty());
    REQUIRE(m.load_hex(text, error));
    m.reset();
    m.run_until_ns(1'200'000'000);
    CHECK(edges(m.take_pin_events(), pb0).size() >= 2);
    CHECK(!m.load_hex(":10000000FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF01\n", error)); // a wrong checksum (00 is right)
    CHECK(error.find("checksum") != std::string::npos);
    CHECK(!m.load_hex("", error)); // a file with no data is not a firmware
    CHECK(error.find("no data") != std::string::npos);

    m.set_eeprom({1, 2, 3});
    m.chip_erase(); // EESAVE unprogrammed (factory): erased
    CHECK(m.eeprom()[0] == 0xFF);
    Atmega::Options keep;
    keep.fuse_high = 0xD9 & ~0x08;
    Atmega k(keep);
    k.set_eeprom({1, 2, 3});
    k.chip_erase();
    CHECK(k.eeprom()[0] == 1);
}

int main(int argc, char** argv)
{
    if (argc < 2) {
        std::fprintf(stderr, "usage: avr_tests <repository-dir> [filter]\n");
        return 2;
    }
    repository = argv[1];
    if (argc > 3) {
        trace_block = std::strtol(argv[3], nullptr, 10);
    }
    return simnodus::test::run_all(argc > 2 ? argv[2] : "");
}
