// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// RV32IMC core of the SimNodus emulator (experiment E-08, SN-243). Directed
// cases whose expected results were worked out from The RISC-V Instruction Set
// Manual (volume I 20191213, volume II 20211203), not from the emulator's own
// output; instruction words come from GNU as (riscv32-esp-elf-as 14.2.0,
// -march=rv32imc_zicsr_zifencei). Then the decoder's robustness (gate G8a.4),
// the architecture test corpus (G8a.1) and the differential corpus recorded
// from QEMU (G8a.2).
//
//   riscv_tests <repository-dir> [filter]
#include "test_support.hpp"

#include "emu/elf.hpp"
#include "emu/riscv_cpu.hpp"
#include "emu/riscv_test_machine.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <map>
#include <random>
#include <string>
#include <vector>

using namespace simnodus::emu;
namespace fs = std::filesystem;

namespace {

fs::path repository;

constexpr std::uint32_t base = 0x80000000u;
constexpr std::uint32_t handler = base + 0x800;
constexpr std::uint32_t data = base + 0x8000;
constexpr std::uint16_t mstatus = 0x300, mie = 0x304, mtvec = 0x305, mcounteren = 0x306, mscratch = 0x340,
    mepc = 0x341, mcause = 0x342, mtval = 0x343, pmpcfg0 = 0x3A0, pmpaddr0 = 0x3B0;

RiscvCpu::Config at_base(RiscvCpu::Config config)
{
    config.reset_pc = base;
    return config;
}

// A bare core with 64 KiB of RAM at 0x80000000, code at its start and the
// trap vector at 0x80000800. Words whose low bits are not 11 are compressed.
struct Rv {
    Bus bus;
    RiscvCpu cpu;

    Rv(const Rv&) = delete;
    Rv& operator=(const Rv&) = delete;

    explicit Rv(std::initializer_list<std::uint32_t> code, RiscvCpu::Config config = {})
        : cpu(bus, at_base(config))
    {
        bus.add_memory("RAM", base, 0x10000, true);
        cpu.add_code_region(base, 0x10000);
        place(base, code);
        cpu.reset();
        cpu.write_csr(mtvec, handler);
    }
    void place(std::uint32_t at, std::initializer_list<std::uint32_t> code)
    {
        for (std::uint32_t word : code) {
            const std::uint32_t size = (word & 3) == 3 ? 4 : 2;
            bus.load(at, reinterpret_cast<const std::uint8_t*>(&word), size);
            at += size;
        }
        cpu.invalidate_code_cache();
    }
    void step(int count = 1)
    {
        for (int i = 0; i < count; ++i) {
            cpu.step_instruction();
        }
    }
    std::uint32_t csr(std::uint16_t number)
    {
        std::uint32_t value = 0;
        CHECK(cpu.read_csr(number, value));
        return value;
    }
    std::uint32_t word(std::uint32_t address)
    {
        std::uint32_t value = 0;
        CHECK(bus.read(address, 4, value));
        return value;
    }
    void set_word(std::uint32_t address, std::uint32_t value) { bus.write(address, 4, value); }
};

constexpr int a0 = 10, a1 = 11, a2 = 12, ra = 1, sp = 2;

// Runs one instruction with a1 and a2 set and returns a0.
std::uint32_t op(std::uint32_t word, std::uint32_t x, std::uint32_t y)
{
    Rv rv({word});
    rv.cpu.set_reg(a1, x);
    rv.cpu.set_reg(a2, y);
    rv.cpu.set_reg(a0, 0xDEADBEEF);
    rv.step();
    CHECK(rv.cpu.pc() == base + 4);
    return rv.cpu.reg(a0);
}

std::uint32_t cop(std::uint32_t word, std::uint32_t x0_value, std::uint32_t x1_value)
{
    Rv rv({word});
    rv.cpu.set_reg(a0, x0_value);
    rv.cpu.set_reg(a1, x1_value);
    rv.step();
    CHECK(rv.cpu.pc() == base + 2);
    return rv.cpu.reg(a0);
}

std::vector<std::uint8_t> read_file(const fs::path& path)
{
    std::ifstream stream(path, std::ios::binary);
    return std::vector<std::uint8_t>((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
}

} // namespace

// ---------------------------------------------------------------------------
// Decoder against the assembler.

TEST_CASE("riscv: decoder matches GNU as encodings")
{
    struct Row {
        std::uint32_t word;
        const char* entry;
        int rd, rs1, rs2;
        std::int32_t imm;
    };
    const Row rows[] = {
        {0x00c58533, "add", a0, a1, a2, 0}, {0x40c58533, "sub", a0, a1, a2, 0},
        {0x40c5d533, "sra", a0, a1, a2, 0}, {0xfff58513, "addi", a0, a1, -1, -1},
        {0x0f05e513, "ori", a0, a1, -1, 240}, {0x41f5d513, "srai", a0, a1, -1, 31},
        {0xfffff537, "lui", a0, 31, -1, static_cast<std::int32_t>(0xfffff000)},
        {0x00001517, "auipc", a0, 0, -1, 0x1000}, {0x008000ef, "jal", ra, 0, -1, 8},
        {0x004580e7, "jalr", ra, a1, -1, 4}, {0xfeb50ee3, "beq", 32, a0, a1, -4},
        {0x00b57463, "bgeu", 32, a0, a1, 8}, {0x00158503, "lb", a0, a1, -1, 1},
        {0x0025d503, "lhu", a0, a1, -1, 2}, {0x00c580a3, "sb", 32, a1, a2, 1},
        {0x00c5a223, "sw", 32, a1, a2, 4}, {0x02c5a533, "mulhsu", a0, a1, a2, 0},
        {0x02c5f533, "remu", a0, a1, a2, 0}, {0x0ff0000f, "fence", 32, 0, -1, 0},
        {0x0000100f, "fence.i", 32, 0, -1, 0}, {0x00000073, "ecall", 32, 0, -1, 0},
        {0x00100073, "ebreak", 32, 0, -1, 0}, {0x30200073, "mret", 32, 0, -1, 0},
        {0x10500073, "wfi", 32, 0, -1, 0}, {0x3402d573, "csrrwi", a0, 5, -1, 5},
        {0x0808, "c.addi4spn", a0, sp, -1, 16}, {0x41c8, "c.lw", a0, a1, -1, 4},
        {0xc1d0, "c.sw", 32, a1, a2, 4}, {0x0001, "c.addi", 32, 0, -1, 0},
        {0x157d, "c.addi", a0, a0, -1, -1}, {0x2021, "c.jal", ra, 0, -1, 8},
        {0x5501, "c.li", a0, 0, -1, -32}, {0x7139, "c.addi16sp", sp, sp, -1, -64},
        {0x657d, "c.lui", a0, 0, -1, 0x1f000}, {0x7501, "c.lui", a0, 0, -1, static_cast<std::int32_t>(0xfffe0000)},
        {0x810d, "c.srli", a0, a0, -1, 3}, {0x850d, "c.srai", a0, a0, -1, 3},
        {0x997d, "c.andi", a0, a0, -1, -1}, {0x8d0d, "c.sub", a0, a0, a1, 0},
        {0x8d6d, "c.and", a0, a0, a1, 0}, {0xa021, "c.j", 32, 0, -1, 8},
        {0xbffd, "c.j", 32, 0, -1, -2}, {0xc501, "c.beqz", 32, a0, -1, 8},
        {0xd101, "c.beqz", 32, a0, -1, -256}, {0xe501, "c.bnez", 32, a0, -1, 8},
        {0x057e, "c.slli", a0, a0, -1, 31}, {0x557e, "c.lwsp", a0, sp, -1, 252},
        {0x8582, "c.jr", 32, a1, -1, 0}, {0x852e, "c.mv", a0, 0, a1, 0},
        {0x9002, "c.ebreak", 32, 0, -1, 0}, {0x9582, "c.jalr", ra, a1, -1, 0},
        {0x952e, "c.add", a0, a0, a1, 0}, {0xdfae, "c.swsp", 32, sp, a1, 252},
    };
    for (const Row& row : rows) {
        const RvInsn in = RiscvCpu::decode(row.word);
        // rs2 is checked where the instruction has one (rs2 >= 0 in the table).
        const bool ok = std::string(RiscvCpu::entry_name(in.entry)) == row.entry && in.rd == row.rd
            && in.rs1 == row.rs1 && in.imm == row.imm && in.size == ((row.word & 3) == 3 ? 4 : 2)
            && (row.rs2 < 0 || in.rs2 == row.rs2);
        if (!ok) {
            char text[160];
            std::snprintf(text, sizeof text, "0x%08x: got %s rd=%d rs1=%d rs2=%d imm=%d", static_cast<unsigned>(row.word),
                RiscvCpu::entry_name(in.entry), in.rd, in.rs1, in.rs2, static_cast<int>(in.imm));
            simnodus::test::fail(__FILE__, __LINE__, text);
        }
    }
    // CSR numbers and reserved encodings.
    CHECK(RiscvCpu::decode(0x34059573).csr == 0x340);
    CHECK(RiscvCpu::decode(0xf1159573).csr == 0xF11);
    CHECK(std::string(RiscvCpu::entry_name(RiscvCpu::decode(0x0000).entry)) == "illegal"); // all-zero halfword
    CHECK(std::string(RiscvCpu::entry_name(RiscvCpu::decode(0x6000).entry)) == "illegal"); // C.FLW: no F
    CHECK(std::string(RiscvCpu::entry_name(RiscvCpu::decode(0x6101).entry)) == "illegal"); // C.ADDI16SP 0
    CHECK(std::string(RiscvCpu::entry_name(RiscvCpu::decode(0x6501).entry)) == "illegal"); // C.LUI imm 0
    CHECK(std::string(RiscvCpu::entry_name(RiscvCpu::decode(0x8082 & ~0x80u).entry)) == "illegal"); // C.JR x0
    CHECK(std::string(RiscvCpu::entry_name(RiscvCpu::decode(0x4002).entry)) == "illegal"); // C.LWSP x0
    CHECK(std::string(RiscvCpu::entry_name(RiscvCpu::decode(0x910d).entry)) == "illegal"); // C.SRLI shamt[5]
    CHECK(std::string(RiscvCpu::entry_name(RiscvCpu::decode(0x9d0d).entry)) == "illegal"); // C.SUBW (RV64)
    CHECK(std::string(RiscvCpu::entry_name(RiscvCpu::decode(0x02059513).entry)) == "illegal"); // SLLI shamt[5]
    CHECK(std::string(RiscvCpu::entry_name(RiscvCpu::decode(0x10200073).entry)) == "illegal"); // SRET
    CHECK(std::string(RiscvCpu::entry_name(RiscvCpu::decode(0x0000001f).entry)) == "illegal"); // 48-bit prefix
}

// ---------------------------------------------------------------------------
// RV32I and M.

TEST_CASE("riscv: register-register operations")
{
    // a1 = 0x80000000, a2 = 33: shifts use the low five bits (1).
    CHECK(op(0x00c58533, 0x80000000, 33) == 0x80000021); // add
    CHECK(op(0x40c58533, 5, 7) == 0xFFFFFFFE);            // sub
    CHECK(op(0x00c59533, 0x80000001, 33) == 0x00000002);  // sll
    CHECK(op(0x00c5a533, 0x80000000, 33) == 1);           // slt: negative < 33
    CHECK(op(0x00c5b533, 0x80000000, 33) == 0);           // sltu
    CHECK(op(0x00c5c533, 0xF0F0F0F0, 0xFF00FF00) == 0x0FF00FF0); // xor
    CHECK(op(0x00c5d533, 0x80000000, 33) == 0x40000000);  // srl
    CHECK(op(0x40c5d533, 0x80000000, 33) == 0xC0000000);  // sra
    CHECK(op(0x00c5e533, 0xF0F0F0F0, 0x0F000000) == 0xFFF0F0F0); // or
    CHECK(op(0x00c5f533, 0xF0F0F0F0, 0xFF00FF00) == 0xF000F000); // and
}

TEST_CASE("riscv: immediate operations")
{
    CHECK(op(0xfff58513, 0, 0) == 0xFFFFFFFF);         // addi -1
    CHECK(op(0xfff5a513, 0x80000000, 0) == 1);         // slti: INT_MIN < -1
    CHECK(op(0xfff5b513, 0xFFFFFFFE, 0) == 1);         // sltiu: -1 compares as 0xFFFFFFFF
    CHECK(op(0xfff5b513, 0xFFFFFFFF, 0) == 0);
    CHECK(op(0xfff5c513, 0x12345678, 0) == 0xEDCBA987); // xori -1 = not
    CHECK(op(0x0f05e513, 0x01, 0) == 0xF1);             // ori
    CHECK(op(0x0f05f513, 0xFF, 0) == 0xF0);             // andi
    CHECK(op(0x01f59513, 3, 0) == 0x80000000);          // slli 31
    CHECK(op(0x01f5d513, 0x80000000, 0) == 1);          // srli 31
    CHECK(op(0x41f5d513, 0x80000000, 0) == 0xFFFFFFFF); // srai 31
    CHECK(op(0xfffff537, 0, 0) == 0xFFFFF000);          // lui
    CHECK(op(0x00001517, 0, 0) == base + 0x1000);       // auipc
}

TEST_CASE("riscv: multiply and divide")
{
    const std::uint32_t m2 = 0xFFFFFFFE; // -2
    CHECK(op(0x02c58533, m2, 3) == 0xFFFFFFFA);        // mul: -6
    CHECK(op(0x02c59533, m2, 3) == 0xFFFFFFFF);        // mulh: high word of -6
    CHECK(op(0x02c5a533, m2, 3) == 0xFFFFFFFF);        // mulhsu: -2 * 3
    CHECK(op(0x02c5b533, m2, 3) == 2);                 // mulhu: 0xFFFFFFFE * 3 = 0x2_FFFFFFFA
    CHECK(op(0x02c5a533, 3, m2) == 2);                 // mulhsu: 3 * 0xFFFFFFFE (unsigned)
    CHECK(op(0x02c59533, 0x80000000, 0x80000000) == 0x40000000); // mulh: 2^62
    CHECK(op(0x02c5c533, m2, 3) == 0);                 // div rounds toward zero
    CHECK(op(0x02c5e533, m2, 3) == m2);                // rem has the dividend's sign
    CHECK(op(0x02c5d533, m2, 3) == 0x55555554);        // divu
    CHECK(op(0x02c5f533, m2, 3) == 2);                 // remu
    // Division by zero and overflow (volume I, table 7.1).
    CHECK(op(0x02c5c533, 7, 0) == 0xFFFFFFFF);
    CHECK(op(0x02c5d533, 7, 0) == 0xFFFFFFFF);
    CHECK(op(0x02c5e533, 7, 0) == 7);
    CHECK(op(0x02c5f533, 7, 0) == 7);
    CHECK(op(0x02c5c533, 0x80000000, 0xFFFFFFFF) == 0x80000000);
    CHECK(op(0x02c5e533, 0x80000000, 0xFFFFFFFF) == 0);
}

TEST_CASE("riscv: jumps and branches")
{
    {
        Rv rv({0x008000ef}); // jal ra, .+8
        rv.step();
        CHECK(rv.cpu.pc() == base + 8);
        CHECK(rv.cpu.reg(ra) == base + 4);
    }
    {
        Rv rv({0x004580e7}); // jalr ra, 4(a1): bit 0 of the target is cleared
        rv.cpu.set_reg(a1, base + 0x101);
        rv.step();
        CHECK(rv.cpu.pc() == base + 0x104);
        CHECK(rv.cpu.reg(ra) == base + 4);
    }
    {
        Rv rv({0x000080e7}); // jalr ra, 0(ra): rs1 is read before rd is written
        rv.cpu.set_reg(ra, base + 0x40);
        rv.step();
        CHECK(rv.cpu.pc() == base + 0x40);
        CHECK(rv.cpu.reg(ra) == base + 4);
    }
    struct Branch {
        std::uint32_t word;
        std::uint32_t x, y;
        bool taken;
    };
    const Branch branches[] = {
        {0x00b50463, 5, 5, true}, {0x00b50463, 5, 6, false},          // beq
        {0x00b51463, 5, 6, true}, {0x00b51463, 5, 5, false},          // bne
        {0x00b54463, 0xFFFFFFFF, 0, true}, {0x00b54463, 0, 0xFFFFFFFF, false}, // blt signed
        {0x00b55463, 0, 0xFFFFFFFF, true}, {0x00b55463, 3, 3, true},  // bge
        {0x00b56463, 0, 0xFFFFFFFF, true}, {0x00b56463, 0xFFFFFFFF, 0, false}, // bltu
        {0x00b57463, 0xFFFFFFFF, 0, true}, {0x00b57463, 0, 1, false}, // bgeu
    };
    for (const Branch& b : branches) {
        Rv rv({b.word});
        rv.cpu.set_reg(a0, b.x);
        rv.cpu.set_reg(a1, b.y);
        rv.step();
        CHECK(rv.cpu.pc() == base + (b.taken ? 8u : 4u));
    }
    Rv back({0x00000013, 0x00000013, 0xfeb50ee3}); // nop, nop, beq a0, a1, .-4
    back.step(3);
    CHECK(back.cpu.pc() == base + 4);
}

TEST_CASE("riscv: loads and stores")
{
    Rv rv({0x00158503, 0x00259503, 0x0045a503, 0x0015c503, 0x0025d503, 0x00c580a3, 0x00c59123, 0x00c5a223});
    rv.set_word(data, 0x80F08000);
    rv.set_word(data + 4, 0x11223344);
    rv.cpu.set_reg(a1, data);
    rv.cpu.set_reg(a2, 0xAABBCCDD);
    rv.step();
    CHECK(rv.cpu.reg(a0) == 0xFFFFFF80); // lb 1(a1): byte 0x80, sign-extended
    rv.step();
    CHECK(rv.cpu.reg(a0) == 0xFFFF80F0); // lh 2(a1)
    rv.step();
    CHECK(rv.cpu.reg(a0) == 0x11223344); // lw 4(a1)
    rv.step();
    CHECK(rv.cpu.reg(a0) == 0x80);       // lbu
    rv.step();
    CHECK(rv.cpu.reg(a0) == 0x80F0);     // lhu
    rv.step(3);                           // sb 1, sh 2, sw 4
    CHECK(rv.word(data) == 0xCCDDDD00);
    CHECK(rv.word(data + 4) == 0xAABBCCDD);
}

TEST_CASE("riscv: misaligned and faulting accesses trap")
{
    {
        Rv rv({0x0045a503}); // lw a0, 4(a1) with a1 = data + 2
        rv.cpu.set_reg(a1, data + 2);
        rv.cpu.set_reg(a0, 7);
        rv.step();
        CHECK(rv.cpu.pc() == handler);
        CHECK(rv.csr(mcause) == RiscvCpu::LoadMisaligned);
        CHECK(rv.csr(mtval) == data + 6);
        CHECK(rv.csr(mepc) == base);
        CHECK(rv.cpu.reg(a0) == 7); // not written
    }
    {
        Rv rv({0x00c59123}); // sh a2, 2(a1) with a1 odd
        rv.cpu.set_reg(a1, data + 1);
        rv.step();
        CHECK(rv.csr(mcause) == RiscvCpu::StoreMisaligned);
        CHECK(rv.csr(mtval) == data + 3);
    }
    {
        Rv rv({0x0045a503, 0x00c5a223}); // lw from, then sw to, an address with nothing
        rv.cpu.set_reg(a1, 0x100);
        rv.step();
        CHECK(rv.csr(mcause) == RiscvCpu::LoadAccessFault);
        CHECK(rv.csr(mtval) == 0x104);
        rv.cpu.set_pc(base + 4);
        rv.step();
        CHECK(rv.csr(mcause) == RiscvCpu::StoreAccessFault);
        CHECK(rv.cpu.instructions() == 0); // neither retired
    }
    {
        Rv rv({0x00058067}); // jalr x0, 0(a1) into nothing: the fetch faults
        rv.cpu.set_reg(a1, 0x200);
        rv.step(2);
        CHECK(rv.cpu.pc() == handler);
        CHECK(rv.csr(mcause) == RiscvCpu::InstructionAccessFault);
        CHECK(rv.csr(mtval) == 0x200);
        CHECK(rv.csr(mepc) == 0x200);
    }
}

TEST_CASE("riscv: compressed instructions")
{
    {
        Rv rv({0x0808}); // c.addi4spn a0, sp, 16
        rv.cpu.set_reg(sp, 0x1000);
        rv.step();
        CHECK(rv.cpu.reg(a0) == 0x1010);
    }
    {
        Rv rv({0x41c8, 0xc1d0, 0x557e, 0xdfae}); // c.lw, c.sw, c.lwsp, c.swsp
        rv.set_word(data + 4, 0x1234);
        rv.set_word(data + 252, 0x5678);
        rv.cpu.set_reg(a1, data);
        rv.cpu.set_reg(a2, 0xABCD);
        rv.cpu.set_reg(sp, data);
        rv.step();
        CHECK(rv.cpu.reg(a0) == 0x1234);
        rv.step();
        CHECK(rv.word(data + 4) == 0xABCD);
        rv.step();
        CHECK(rv.cpu.reg(a0) == 0x5678);
        rv.step();
        CHECK(rv.word(data + 252) == data); // c.swsp a1
    }
    CHECK(cop(0x157d, 0, 0) == 0xFFFFFFFF);         // c.addi a0, -1
    CHECK(cop(0x5501, 0, 0) == 0xFFFFFFE0);         // c.li a0, -32
    CHECK(cop(0x657d, 0, 0) == 0x1F000);            // c.lui a0, 0x1f
    CHECK(cop(0x7501, 0, 0) == 0xFFFE0000);         // c.lui a0, 0xfffe0
    CHECK(cop(0x810d, 0x80000010, 0) == 0x10000002); // c.srli 3
    CHECK(cop(0x850d, 0x80000010, 0) == 0xF0000002); // c.srai 3
    CHECK(cop(0x997d, 0x1234, 0) == 0x1234);        // c.andi -1
    CHECK(cop(0x8d0d, 5, 7) == 0xFFFFFFFE);         // c.sub
    CHECK(cop(0x8d2d, 0xF0, 0xFF) == 0x0F);         // c.xor
    CHECK(cop(0x8d4d, 0xF0, 0x0F) == 0xFF);         // c.or
    CHECK(cop(0x8d6d, 0xF0, 0x3C) == 0x30);         // c.and
    CHECK(cop(0x057e, 3, 0) == 0x80000000);         // c.slli 31
    CHECK(cop(0x852e, 9, 42) == 42);                // c.mv
    CHECK(cop(0x952e, 9, 42) == 51);                // c.add
    {
        Rv rv({0x7139}); // c.addi16sp sp, -64
        rv.cpu.set_reg(sp, 0x1000);
        rv.step();
        CHECK(rv.cpu.reg(sp) == 0x1000 - 64);
    }
    {
        Rv rv({0x0001}); // c.nop
        rv.step();
        CHECK(rv.cpu.pc() == base + 2);
    }
    {
        Rv rv({0x2021}); // c.jal .+8
        rv.step();
        CHECK(rv.cpu.pc() == base + 8);
        CHECK(rv.cpu.reg(ra) == base + 2);
    }
    {
        Rv rv({0x0001, 0xbffd}); // c.nop; c.j .-2
        rv.step(2);
        CHECK(rv.cpu.pc() == base);
    }
    {
        Rv rv({0xc501, 0xe501}); // c.beqz a0, .+8 not taken; c.bnez a0, .+8 taken
        rv.cpu.set_reg(a0, 1);
        rv.step();
        CHECK(rv.cpu.pc() == base + 2);
        rv.step();
        CHECK(rv.cpu.pc() == base + 10);
    }
    {
        Rv rv({0x8582}); // c.jr a1
        rv.cpu.set_reg(a1, base + 0x20);
        rv.step();
        CHECK(rv.cpu.pc() == base + 0x20);
    }
    {
        Rv rv({0x9082}); // c.jalr ra: the target is read before ra is written
        rv.cpu.set_reg(ra, base + 0x30);
        rv.step();
        CHECK(rv.cpu.pc() == base + 0x30);
        CHECK(rv.cpu.reg(ra) == base + 2);
    }
    {
        Rv rv({0x9002}); // c.ebreak
        rv.step();
        CHECK(rv.cpu.pc() == handler);
        CHECK(rv.csr(mcause) == RiscvCpu::Breakpoint);
        CHECK(rv.csr(mtval) == base);
    }
    {
        Rv rv({0x0000}); // the all-zero halfword is illegal
        rv.step();
        CHECK(rv.csr(mcause) == RiscvCpu::IllegalInstruction);
        CHECK(rv.csr(mtval) == 0);
    }
}

TEST_CASE("riscv: x0 stays zero")
{
    Rv rv({0x00500013}); // addi x0, x0, 5
    rv.step();
    CHECK(rv.cpu.reg(0) == 0);
    Rv lui({0xfffff037}); // lui x0, 0xfffff
    lui.step();
    CHECK(lui.cpu.reg(0) == 0);
}

// ---------------------------------------------------------------------------
// CSRs, traps and privilege.

TEST_CASE("riscv: CSR instructions")
{
    Rv rv({0x34059573, 0x3405b573, 0x3402d573, 0x34016573, 0x3400f573, 0x30102573, 0x30159073, 0xf1402573});
    rv.cpu.write_csr(mscratch, 0xF0F0);
    rv.cpu.set_reg(a1, 0x00FF);
    rv.step(); // csrrw a0, mscratch, a1
    CHECK(rv.cpu.reg(a0) == 0xF0F0);
    CHECK(rv.csr(mscratch) == 0x00FF);
    rv.cpu.set_reg(a1, 0x000F);
    rv.step(); // csrrc a0, mscratch, a1
    CHECK(rv.cpu.reg(a0) == 0x00FF);
    CHECK(rv.csr(mscratch) == 0x00F0);
    rv.step(); // csrrwi a0, mscratch, 5
    CHECK(rv.cpu.reg(a0) == 0x00F0);
    CHECK(rv.csr(mscratch) == 5);
    rv.step(); // csrrsi a0, mscratch, 2
    CHECK(rv.csr(mscratch) == 7);
    rv.step(); // csrrci a0, mscratch, 1
    CHECK(rv.csr(mscratch) == 6);
    rv.step(); // csrrs a0, misa, x0: RV32 I, M, C, U
    CHECK(rv.cpu.reg(a0) == ((1u << 30) | (1u << 8) | (1u << 12) | (1u << 2) | (1u << 20)));
    rv.step(); // csrrw x0, misa, a1: misa is fixed (WARL), no trap
    CHECK(rv.cpu.pc() == base + 28);
    CHECK(rv.csr(0x301) == ((1u << 30) | (1u << 8) | (1u << 12) | (1u << 2) | (1u << 20)));
    rv.step(); // csrrs a0, mhartid, x0
    CHECK(rv.cpu.reg(a0) == 0);

    Rv ro({0xf1159573}); // csrrw a0, mvendorid, a1: writing a read-only CSR is illegal
    ro.step();
    CHECK(ro.csr(mcause) == RiscvCpu::IllegalInstruction);
    CHECK(ro.csr(mtval) == 0xf1159573);
    Rv missing({0x7c002573}); // csrrs a0, 0x7c0, x0: no such CSR
    missing.step();
    CHECK(missing.csr(mcause) == RiscvCpu::IllegalInstruction);
    Rv custom({0x7c002573});
    custom.cpu.read_custom_csr = [](std::uint16_t number, std::uint32_t& value) {
        value = number == 0x7c0 ? 0x5A : 0;
        return number == 0x7c0;
    };
    custom.step();
    CHECK(custom.cpu.reg(a0) == 0x5A);
}

TEST_CASE("riscv: ecall, mret and privilege")
{
    Rv rv({0x00000073});
    rv.cpu.write_csr(mstatus, 0x8); // MIE
    rv.step();
    CHECK(rv.cpu.pc() == handler);
    CHECK(rv.csr(mcause) == RiscvCpu::EcallFromM);
    CHECK(rv.csr(mepc) == base);
    CHECK(rv.csr(mstatus) == ((3u << 11) | 0x80)); // MPP = M, MPIE = 1, MIE = 0

    // mret to user mode, then ecall from U, and machine-only instructions trap.
    Rv user({0x30200073});
    user.place(base + 0x100, {0x00000073});
    user.place(base + 0x200, {0x30200073});
    user.place(base + 0x300, {0x30002573});
    user.cpu.write_csr(mepc, base + 0x100);
    user.cpu.write_csr(mstatus, 0x80); // MPP = U, MPIE = 1
    user.cpu.write_csr(pmpaddr0, 0xFFFFFFFF); // NAPOT over everything, RWX, so user code may run
    user.cpu.write_csr(pmpcfg0, 0x1F);
    user.step(); // mret
    CHECK(user.cpu.privilege() == RiscvCpu::Privilege::User);
    CHECK(user.cpu.pc() == base + 0x100);
    CHECK(user.csr(mstatus) == (0x8u | 0x80u)); // MIE = MPIE = 1, MPP = U
    user.step(); // ecall from U
    CHECK(user.csr(mcause) == RiscvCpu::EcallFromU);
    CHECK(user.cpu.privilege() == RiscvCpu::Privilege::Machine);
    CHECK(((user.csr(mstatus) >> 11) & 3) == 0); // MPP = U
    for (std::uint32_t at : {base + 0x200, base + 0x300}) { // mret, csrrs mstatus from U
        user.cpu.write_csr(mepc, at);
        user.cpu.write_csr(mstatus, 0);
        user.cpu.set_pc(base);
        user.step();
        CHECK(user.cpu.privilege() == RiscvCpu::Privilege::User);
        user.step();
        CHECK(user.csr(mcause) == RiscvCpu::IllegalInstruction);
        CHECK(user.csr(mepc) == at);
    }
}

TEST_CASE("riscv: interrupts")
{
    {
        Rv rv({0x00000013, 0x00000013});
        rv.cpu.write_csr(mie, 1u << 7);
        rv.cpu.set_interrupt_lines(1u << 7);
        rv.step(); // MIE = 0 in machine mode: not taken
        CHECK(rv.cpu.pc() == base + 4);
        rv.cpu.write_csr(mstatus, 0x8);
        rv.step(); // taken before the next instruction
        CHECK(rv.cpu.pc() == handler);
        CHECK(rv.csr(mcause) == 0x80000007);
        CHECK(rv.csr(mepc) == base + 4);
        CHECK((rv.csr(mstatus) & 0x88) == 0x80);
    }
    {
        Rv rv({0x00000013}); // vectored mtvec: base + 4 * cause
        rv.cpu.write_csr(mtvec, handler | 1);
        rv.cpu.write_csr(mie, 1u << 11);
        rv.cpu.write_csr(mstatus, 0x8);
        rv.cpu.set_interrupt_lines(1u << 11);
        rv.step();
        CHECK(rv.cpu.pc() == handler + 44);
    }
    {
        // Several pending: the architecture's order is 11, 3, 7; the machine
        // may choose instead (the ESP32-C3's priorities).
        Rv rv({0x00000013});
        rv.cpu.write_csr(mie, (1u << 3) | (1u << 7));
        rv.cpu.write_csr(mstatus, 0x8);
        rv.cpu.set_interrupt_lines((1u << 3) | (1u << 7));
        rv.step();
        CHECK(rv.csr(mcause) == 0x80000003);
        Rv chosen({0x00000013}, [] {
            RiscvCpu::Config c;
            c.interrupt_lines = 0xFFFFFFFE;
            return c;
        }());
        chosen.cpu.choose_interrupt = [](std::uint32_t ready) { return ready & (1u << 20) ? 20 : -1; };
        chosen.cpu.write_csr(mie, 0xFFFFFFFE);
        chosen.cpu.write_csr(mstatus, 0x8);
        chosen.cpu.set_interrupt_lines((1u << 3) | (1u << 20));
        chosen.step();
        CHECK(chosen.csr(mcause) == 0x80000014);
    }
    {
        // WFI sleeps until an enabled line rises, then continues after it
        // even with MIE = 0.
        Rv rv({0x10500073, 0x00000013});
        rv.cpu.write_csr(mie, 1u << 7);
        CHECK(rv.cpu.run(100) == RvState::Sleeping);
        CHECK(rv.cpu.cycles() == 100); // asleep: time moved to the limit
        CHECK(rv.cpu.pc() == base + 4);
        rv.cpu.set_interrupt_lines(1u << 7);
        rv.step();
        CHECK(rv.cpu.state() == RvState::Running);
        CHECK(rv.cpu.pc() == base + 8);
    }
}

TEST_CASE("riscv: physical memory protection")
{
    // User mode with no entry that matches: every access fails.
    Rv rv({0x30200073});
    rv.place(base + 0x100, {0x0045a503});
    rv.cpu.write_csr(mepc, base + 0x100);
    rv.cpu.write_csr(mstatus, 0);
    rv.cpu.write_csr(pmpaddr0, (base >> 2) | 0x7FF); // NAPOT, 16 KiB of code at base: R and X
    rv.cpu.write_csr(pmpcfg0, 0x18 | 0x5);
    rv.cpu.set_reg(a1, data);
    rv.step(2);
    CHECK(rv.csr(mcause) == RiscvCpu::LoadAccessFault);
    CHECK(rv.csr(mtval) == data + 4);
    // A TOR entry from data to data + 0x100, read-only: the load passes.
    rv.cpu.write_csr(pmpaddr0 + 1, data >> 2);
    rv.cpu.write_csr(pmpaddr0 + 2, (data + 0x100) >> 2);
    rv.cpu.write_csr(pmpcfg0, (0x18u | 0x5u) | (0x08u << 16 | 0x1u << 16));
    rv.set_word(data + 4, 99);
    rv.cpu.write_csr(mepc, base + 0x100);
    rv.cpu.write_csr(mstatus, 0);
    rv.cpu.set_pc(base);
    rv.step(2);
    CHECK(rv.cpu.reg(a0) == 99);
    CHECK(rv.cpu.pc() == base + 0x104);
    // Code outside the executable entry cannot run in user mode.
    rv.step(); // the zero halfword after the load is illegal: back to machine mode
    CHECK(rv.cpu.privilege() == RiscvCpu::Privilege::Machine);
    rv.place(data, {0x00000013});
    rv.cpu.write_csr(mepc, data);
    rv.cpu.write_csr(mstatus, 0);
    rv.cpu.set_pc(base);
    rv.step(2);
    CHECK(rv.csr(mcause) == RiscvCpu::InstructionAccessFault);
    CHECK(rv.csr(mepc) == data);

    // A locked entry binds machine mode too and ignores later writes.
    Rv locked({0x0045a503});
    locked.cpu.write_csr(pmpaddr0, (data >> 2) | 0x1); // NAPOT, 16 bytes at data
    locked.cpu.write_csr(pmpcfg0, 0x80 | 0x18);        // L, no permissions
    locked.cpu.write_csr(pmpcfg0, 0x1F);
    locked.cpu.write_csr(pmpaddr0, 0);
    CHECK(locked.csr(pmpcfg0) == 0x98);
    CHECK(locked.csr(pmpaddr0) == ((data >> 2) | 0x1));
    locked.cpu.set_reg(a1, data);
    locked.step();
    CHECK(locked.csr(mcause) == RiscvCpu::LoadAccessFault);
}

TEST_CASE("riscv: counters")
{
    Rv rv({0x00000013, 0x00000013, 0xb0202573, 0xb0259073, 0xb0202573});
    rv.step(3); // two nops retire before minstret is read
    CHECK(rv.cpu.reg(a0) == 2);
    rv.cpu.set_reg(a1, 1000);
    rv.step(2); // csrrw x0, minstret, a1: the writing instruction does not count
    CHECK(rv.cpu.reg(a0) == 1000);

    // cycle and instret from user mode need mcounteren.
    Rv user({0x30200073});
    user.place(base + 0x100, {0xc0202573});
    user.cpu.write_csr(pmpaddr0, 0xFFFFFFFF);
    user.cpu.write_csr(pmpcfg0, 0x1F);
    for (std::uint32_t enable : {0u, 4u}) {
        user.cpu.write_csr(mcounteren, enable);
        user.cpu.write_csr(mepc, base + 0x100);
        user.cpu.write_csr(mstatus, 0);
        user.cpu.set_pc(base);
        user.step(2);
        CHECK((user.cpu.pc() == handler) == (enable == 0));
    }
}

TEST_CASE("riscv: stores into code take effect")
{
    // sw a2, 4(a1) overwrites the next instruction; the decoded copy is dropped.
    Rv rv({0x00c5a223, 0x00000013});
    rv.cpu.set_reg(a1, base);
    rv.cpu.set_reg(a2, 0x02a00513); // addi a0, x0, 42
    rv.step(2);
    CHECK(rv.cpu.reg(a0) == 42);
    // A loader write outside the core is seen after fence.i.
    Rv fence({0x0000100f, 0x00000013});
    fence.cpu.set_pc(base + 4);
    fence.step(); // decodes and caches the nop
    const std::uint32_t li = 0x00700513; // addi a0, x0, 7
    fence.bus.load(base + 4, reinterpret_cast<const std::uint8_t*>(&li), 4);
    fence.cpu.set_pc(base);
    fence.step(2); // fence.i drops the cached nop
    CHECK(fence.cpu.reg(a0) == 7);
}

TEST_CASE("riscv: no code at the trap vector halts the core")
{
    Rv rv({0x0000});
    rv.cpu.write_csr(mtvec, 0x300); // nothing there
    CHECK(rv.cpu.run(1000) == RvState::Halted);
    CHECK(!rv.cpu.halt_reason().empty());
    CHECK(rv.cpu.cycles() == 1000); // time still moves on
}

// ---------------------------------------------------------------------------
// Gate G8a.4: decoding never crashes; random code never hangs the host.

TEST_CASE("riscv: decoder robustness")
{
    for (std::uint32_t h = 0; h < 0x10000; ++h) {
        const RvInsn in = RiscvCpu::decode(h);
        if ((h & 3) != 3) {
            REQUIRE(in.size == 2);
        }
        REQUIRE(in.entry > 0 && in.entry < RiscvCpu::entry_count());
        REQUIRE(in.rd <= 32 && in.rs1 < 32 && in.rs2 < 32);
    }
    std::mt19937 random(12345);
    for (int i = 0; i < 1'000'000; ++i) {
        const std::uint32_t w = static_cast<std::uint32_t>(random()) | 3u;
        const RvInsn in = RiscvCpu::decode(w);
        REQUIRE(in.size == 4);
        REQUIRE(in.entry > 0 && in.entry < RiscvCpu::entry_count());
        REQUIRE(in.rd <= 32 && in.rs1 < 32 && in.rs2 < 32);
    }
    for (int image = 0; image < 200; ++image) {
        RiscvTestMachine machine;
        std::vector<std::uint8_t> code(1024);
        for (auto& byte : code) {
            byte = static_cast<std::uint8_t>(random());
        }
        machine.bus().load(RiscvTestMachine::ram_base, code.data(), static_cast<std::uint32_t>(code.size()));
        machine.cpu().invalidate_code_cache();
        machine.run(5000);
        CHECK(machine.cpu().cycles() >= 5000 || machine.finished());
    }
}

// ---------------------------------------------------------------------------
// The test machine.

TEST_CASE("riscv: test machine timer, UART and finisher")
{
    // Program: set mtimecmp = 10 (1 us at 10 MHz), enable MTIE and MIE, wfi;
    // the handler prints 'T' and finishes.
    RiscvTestMachine machine;
    Bus& bus = machine.bus();
    RiscvCpu& cpu = machine.cpu();
    const std::uint32_t program[] = {
        0x02004537, // lui a0, 0x2004 (CLINT mtimecmp)
        0x00a00593, // li a1, 10
        0x00b52023, // sw a1, 0(a0)
        0x00052223, // sw x0, 4(a0)
        0x08000593, // li a1, 0x80
        0x30459073, // csrw mie, a1
        0x30046073, // csrsi mstatus, 8
        0x10500073, // wfi
        0x0000006f, // j .
    };
    const std::uint32_t isr[] = {
        0x10000537, // lui a0, 0x10000 (UART)
        0x05400593, // li a1, 'T'
        0x00b50023, // sb a1, 0(a0)
        0x00100537, // lui a0, 0x100 (finisher)
        0x000055b7, // lui a1, 5
        0x55558593, // addi a1, a1, 0x555
        0x00b52023, // sw a1, 0(a0)
        0x0000006f, // j .
    };
    bus.load(RiscvTestMachine::ram_base, reinterpret_cast<const std::uint8_t*>(program), sizeof program);
    bus.load(RiscvTestMachine::ram_base + 0x100, reinterpret_cast<const std::uint8_t*>(isr), sizeof isr);
    cpu.invalidate_code_cache();
    cpu.write_csr(mtvec, RiscvTestMachine::ram_base + 0x100);
    CHECK(machine.run(1'000'000));
    CHECK(machine.exit_code() == 0);
    CHECK(machine.output() == "T");
    // 1 us at 160 MHz: the interrupt comes at cycle 160 (asleep before it).
    CHECK(cpu.cycles() >= 160 && cpu.cycles() < 180);
}

// ---------------------------------------------------------------------------
// Gate G8a.1: riscv-arch-test 2.7.4 (tests/experiments/riscv-arch-test).

TEST_CASE("riscv-arch: architecture tests")
{
    const fs::path corpus = repository / "tests" / "experiments" / "riscv-arch-test" / "corpus";
    REQUIRE(fs::is_directory(corpus));
    int passed = 0;
    int total = 0;
    std::vector<fs::path> elfs;
    for (const auto& entry : fs::recursive_directory_iterator(corpus)) {
        if (entry.path().extension() == ".elf") {
            elfs.push_back(entry.path());
        }
    }
    std::sort(elfs.begin(), elfs.end());
    for (const fs::path& path : elfs) {
        ++total;
        std::string error;
        const auto image = read_elf(path, error, ElfMachine::Riscv);
        RiscvTestMachine machine;
        if (!image || !machine.load(*image, error)) {
            simnodus::test::fail(__FILE__, __LINE__, path.filename().string() + ": " + error);
            continue;
        }
        const bool finished = machine.run(50'000'000);
        const auto begin = image->symbol("begin_signature");
        const auto end = image->symbol("end_signature");
        fs::path reference = path;
        reference.replace_extension(".reference_output");
        const std::vector<std::uint8_t> expected_text = read_file(reference);
        std::vector<std::uint32_t> expected;
        std::string line;
        for (std::uint8_t c : expected_text) {
            if (c == '\n') {
                if (!line.empty()) {
                    expected.push_back(static_cast<std::uint32_t>(std::stoul(line, nullptr, 16)));
                }
                line.clear();
            } else if (c != '\r') {
                line.push_back(static_cast<char>(c));
            }
        }
        if (!line.empty()) {
            expected.push_back(static_cast<std::uint32_t>(std::stoul(line, nullptr, 16)));
        }
        bool ok = finished && begin && end && !expected.empty() && *end - *begin >= expected.size() * 4;
        std::size_t first_bad = 0;
        for (std::size_t i = 0; ok && i < expected.size(); ++i) {
            std::uint32_t value = 0;
            machine.bus().read(*begin + static_cast<std::uint32_t>(4 * i), 4, value);
            if (value != expected[i]) {
                ok = false;
                first_bad = i;
            }
        }
        if (ok) {
            ++passed;
        } else {
            std::uint32_t got = 0;
            if (begin) {
                machine.bus().read(*begin + static_cast<std::uint32_t>(4 * first_bad), 4, got);
            }
            char text[256];
            std::snprintf(text, sizeof text, "%s: finished=%d words=%zu first difference at word %zu (got %08x, expected %08x)",
                path.filename().string().c_str(), finished ? 1 : 0, expected.size(), first_bad, static_cast<unsigned>(got),
                static_cast<unsigned>(first_bad < expected.size() ? expected[first_bad] : 0));
            simnodus::test::fail(__FILE__, __LINE__, text);
        }
    }
    std::printf("  architecture tests: %d of %d signatures match\n", passed, total);
    CHECK(total > 0);
}

// ---------------------------------------------------------------------------
// Gate G8a.2: random blocks against QEMU's recorded output
// (tests/experiments/riscv-differential).

TEST_CASE("riscv-diff: differential corpus against QEMU")
{
    const fs::path corpus = repository / "tests" / "experiments" / "riscv-differential" / "corpus";
    REQUIRE(fs::is_directory(corpus));
    std::vector<fs::path> elfs;
    for (const auto& entry : fs::directory_iterator(corpus)) {
        if (entry.path().extension() == ".elf") {
            elfs.push_back(entry.path());
        }
    }
    std::sort(elfs.begin(), elfs.end());
    REQUIRE(!elfs.empty());
    std::vector<std::uint64_t> hits(static_cast<std::size_t>(RiscvCpu::entry_count()), 0);
    for (const fs::path& path : elfs) {
        std::string error;
        const auto image = read_elf(path, error, ElfMachine::Riscv);
        RiscvTestMachine machine;
        REQUIRE(image && machine.load(*image, error));
        machine.cpu().count_entries(true);
        CHECK(machine.run(500'000'000));
        fs::path qemu = path;
        qemu.replace_extension(".qemu.bin");
        const std::vector<std::uint8_t> expected = read_file(qemu);
        const std::string& got = machine.output();
        CHECK(!expected.empty());
        std::size_t differing = 0;
        std::size_t first = 0;
        const std::size_t words = std::min(expected.size(), got.size()) / 4;
        for (std::size_t w = 0; w < words; ++w) {
            if (std::memcmp(expected.data() + 4 * w, got.data() + 4 * w, 4) != 0) {
                if (differing++ == 0) {
                    first = w;
                }
            }
        }
        if (differing != 0 || expected.size() != got.size()) {
            char text[200];
            std::snprintf(text, sizeof text, "%s: %zu differing words (first: word %zu = block %zu), sizes %zu and %zu",
                path.filename().string().c_str(), differing, first, first / 48, expected.size(), got.size());
            simnodus::test::fail(__FILE__, __LINE__, text);
        }
        const auto& counted = machine.cpu().entry_hits();
        for (std::size_t i = 0; i < hits.size(); ++i) {
            hits[i] += counted[i];
        }
    }
    // Coverage (for the report): entries the random blocks do not reach are
    // covered by the directed cases above.
    std::string low;
    for (std::size_t i = 2; i < hits.size(); ++i) {
        if (hits[i] < 1000) {
            low += std::string(" ") + RiscvCpu::entry_name(static_cast<std::uint8_t>(i)) + "=" + std::to_string(hits[i]);
        }
    }
    std::printf("  decoder entries below 1000 hits:%s\n", low.empty() ? " none" : low.c_str());
}

int main(int argc, char** argv)
{
    if (argc < 2) {
        std::fprintf(stderr, "usage: riscv_tests <repository-dir> [filter]\n");
        return 2;
    }
    repository = argv[1];
    return simnodus::test::run_all(argc > 2 ? argv[2] : "");
}
