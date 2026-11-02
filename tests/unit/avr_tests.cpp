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

#include "emu/avr_cpu.hpp"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
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
