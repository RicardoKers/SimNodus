// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// SimNodus STM32 emulator (experiment E-07, SN-170): instruction vectors whose
// expected results were worked out from the ARMv7-M pseudocode (DDI 0403E),
// not from the emulator's own output; encodings come from GNU as
// (arm-none-eabi-as -mcpu=cortex-m3). Then the exception model and the
// STM32F103 machine with the example firmware.
//
//   emu_tests <examples-dir> [filter]
#include "test_support.hpp"

#include "emu/cpu.hpp"
#include "emu/elf.hpp"
#include "emu/stm32f103.hpp"

#include <cmath>
#include <cstring>
#include <filesystem>
#include <initializer_list>
#include <memory>
#include <string>
#include <vector>

using namespace simnodus::emu;
namespace fs = std::filesystem;

namespace {

fs::path examples;

constexpr std::uint32_t code_base = 0x08000100;
constexpr std::uint32_t hardfault_handler = 0x08000200;
constexpr std::uint32_t svc_handler = 0x08000210;
constexpr std::uint32_t systick_handler = 0x08000220;
constexpr std::uint32_t usage_handler = 0x08000230;
constexpr std::uint32_t stack_top = 0x20004000;
constexpr std::uint32_t ram = 0x20000100;

// A bare Cortex-M3 with 64 KiB flash, 20 KiB SRAM and the SCS; code at 0x08000100.
struct Core {
    Bus bus;
    Cpu cpu{bus};

    Core(const Core&) = delete;
    Core& operator=(const Core&) = delete;

    explicit Core(std::initializer_list<std::uint16_t> code)
    {
        bus.add_memory("flash", 0x08000000, 0x10000, false);
        bus.add_alias("boot alias", 0, "flash");
        bus.add_memory("SRAM", 0x20000000, 0x5000, true);
        bus.add_bit_band(0x22000000, 0x5000 * 32, 0x20000000);
        bus.add_device(0xE000E000, 0x1000, &cpu.scs());
        std::vector<std::uint32_t> vectors(16 + 16, 0);
        vectors[0] = stack_top;
        vectors[1] = code_base | 1;
        vectors[3] = hardfault_handler | 1;
        vectors[6] = usage_handler | 1;
        vectors[11] = svc_handler | 1;
        vectors[15] = systick_handler | 1;
        bus.load(0x08000000, reinterpret_cast<const std::uint8_t*>(vectors.data()),
            static_cast<std::uint32_t>(vectors.size() * 4));
        const std::uint16_t loop = 0xE7FE; // b .
        for (std::uint32_t handler : {hardfault_handler, svc_handler, systick_handler, usage_handler}) {
            bus.load(handler, reinterpret_cast<const std::uint8_t*>(&loop), 2);
        }
        std::vector<std::uint16_t> words(code);
        bus.load(code_base, reinterpret_cast<const std::uint8_t*>(words.data()), static_cast<std::uint32_t>(words.size() * 2));
        cpu.reset();
    }

    void steps(int n) { cpu.run(cpu.cycles() + static_cast<std::uint64_t>(n)); }
    std::uint32_t r(int n) const { return cpu.reg(n); }
    void set(int n, std::uint32_t v) { cpu.set_reg(n, v); }
    bool flag_n() const { return (cpu.xpsr() >> 31) & 1u; }
    bool flag_z() const { return (cpu.xpsr() >> 30) & 1u; }
    bool flag_c() const { return (cpu.xpsr() >> 29) & 1u; }
    bool flag_v() const { return (cpu.xpsr() >> 28) & 1u; }
    bool flag_q() const { return (cpu.xpsr() >> 27) & 1u; }
    // Sets the flags through MSR APSR (runs 2 instructions placed after `code`).
    std::uint32_t word(std::uint32_t address)
    {
        std::uint32_t value = 0;
        bus.read(address, 4, value);
        return value;
    }
    void poke(std::uint32_t address, std::uint32_t value) { bus.write(address, 4, value); }
};

// Runs `code` with registers preset; flags from `apsr` (NZCV in bits 31..28).
std::unique_ptr<Core> run(std::initializer_list<std::uint16_t> code,
    std::initializer_list<std::pair<int, std::uint32_t>> regs, int instructions, std::uint32_t apsr = 0)
{
    // Prefix: MSR APSR_nzcvq, r12 with r12 = apsr.
    std::vector<std::uint16_t> all = {0xF38C, 0x8800};
    all.insert(all.end(), code.begin(), code.end());
    auto core = std::make_unique<Core>(std::initializer_list<std::uint16_t>{});
    core->bus.load(code_base, reinterpret_cast<const std::uint8_t*>(all.data()), static_cast<std::uint32_t>(all.size() * 2));
    core->cpu.reset();
    core->set(12, apsr);
    for (const auto& [n, v] : regs) {
        core->set(n, v);
    }
    core->steps(1 + instructions);
    return core;
}

constexpr std::uint32_t N = 1u << 31, Z = 1u << 30, C = 1u << 29, V = 1u << 28;

} // namespace

TEST_CASE("16-bit add and subtract set N Z C V as AddWithCarry says")
{
    auto c = run({0x1888}, {{1, 0x7FFFFFFF}, {2, 1}}, 1); // adds r0, r1, r2
    CHECK(c->r(0) == 0x80000000u);
    CHECK(c->flag_n() && !c->flag_z() && !c->flag_c() && c->flag_v());
    c = run({0x1A88}, {{1, 0}, {2, 1}}, 1); // subs r0, r1, r2: borrow -> C clear
    CHECK(c->r(0) == 0xFFFFFFFFu);
    CHECK(c->flag_n() && !c->flag_c() && !c->flag_v());
    c = run({0x1A88}, {{1, 5}, {2, 5}}, 1);
    CHECK(c->r(0) == 0 && c->flag_z() && c->flag_c());
    c = run({0x4148}, {{0, 0xFFFFFFFF}, {1, 0}}, 1, C); // adcs r0, r1 with carry in
    CHECK(c->r(0) == 0 && c->flag_z() && c->flag_c());
    c = run({0x4188}, {{0, 5}, {1, 3}}, 1, 0); // sbcs r0, r1 with C=0: 5 - 3 - 1
    CHECK(c->r(0) == 1 && c->flag_c());
    c = run({0x4248}, {{1, 1}}, 1); // negs r0, r1
    CHECK(c->r(0) == 0xFFFFFFFFu && c->flag_n() && !c->flag_c());
    c = run({0x4248}, {{1, 0}}, 1);
    CHECK(c->r(0) == 0 && c->flag_z() && c->flag_c());
}

TEST_CASE("shifts by immediate and register give the architectural carry")
{
    auto c = run({0x07C8}, {{1, 3}}, 1); // lsls r0, r1, #31: carry = bit 1
    CHECK(c->r(0) == 0x80000000u && c->flag_c() && c->flag_n());
    c = run({0x0808}, {{1, 0x80000000}}, 1); // lsrs r0, r1, #32
    CHECK(c->r(0) == 0 && c->flag_c() && c->flag_z());
    c = run({0x1008}, {{1, 0x80000000}}, 1); // asrs r0, r1, #32
    CHECK(c->r(0) == 0xFFFFFFFFu && c->flag_c());
    c = run({0x41C8}, {{0, 0x12345678}, {1, 8}}, 1); // rors r0, r1
    CHECK(c->r(0) == 0x78123456u && !c->flag_c());
    c = run({0x41C8}, {{0, 0x12345678}, {1, 36}}, 1); // rotate by 36 = 4
    CHECK(c->r(0) == 0x81234567u && c->flag_c());
    c = run({0x41C8}, {{0, 0x80000000}, {1, 32}}, 1); // by 32: value kept, C = bit 31
    CHECK(c->r(0) == 0x80000000u && c->flag_c());
    c = run({0x41C8}, {{0, 0x12345678}, {1, 0}}, 1, C); // by 0: C unchanged
    CHECK(c->r(0) == 0x12345678u && c->flag_c());
}

TEST_CASE("32-bit data processing with modified immediates and shifted registers")
{
    auto c = run({0xF101, 0x10AB}, {{1, 1}}, 1, Z); // add.w r0, r1, #0x00AB00AB (no flags)
    CHECK(c->r(0) == 0x00AB00ACu && c->flag_z());
    c = run({0xF05F, 0x4000}, {}, 1); // movs.w r0, #0x80000000: carry from the rotation
    CHECK(c->r(0) == 0x80000000u && c->flag_n() && c->flag_c());
    c = run({0xEA11, 0x2032}, {{1, 0xFFFFFFFF}, {2, 0xFF}}, 1); // ands.w r0, r1, r2, ror #8
    CHECK(c->r(0) == 0xFF000000u && c->flag_c() && c->flag_n());
    c = run({0xEA41, 0x0032}, {{1, 0}, {2, 2}}, 1, C); // orr.w r0, r1, r2, rrx
    CHECK(c->r(0) == 0x80000001u);
    c = run({0xEA6F, 0x1001}, {{1, 0x0F000001}}, 1); // mvn.w r0, r1, lsl #4
    CHECK(c->r(0) == 0x0FFFFFEFu);
    c = run({0xF5B0, 0x7F80}, {{0, 0x100}}, 1); // cmp.w r0, #0x100
    CHECK(c->flag_z() && c->flag_c() && !c->flag_n());
}

TEST_CASE("multiply, multiply-accumulate and divide")
{
    auto c = run({0xFBA2, 0x0103}, {{2, 0xFFFFFFFF}, {3, 2}}, 1); // umull r0, r1, r2, r3
    CHECK(c->r(0) == 0xFFFFFFFEu && c->r(1) == 1);
    c = run({0xFB82, 0x0103}, {{2, 0xFFFFFFFE}, {3, 3}}, 1); // smull: -2 * 3
    CHECK(c->r(0) == 0xFFFFFFFAu && c->r(1) == 0xFFFFFFFFu);
    c = run({0xFBE2, 0x0103}, {{0, 0xFFFFFFFF}, {1, 0}, {2, 1}, {3, 1}}, 1); // umlal
    CHECK(c->r(0) == 0 && c->r(1) == 1);
    c = run({0xFBC2, 0x0103}, {{0, 0}, {1, 0}, {2, 0xFFFFFFFF}, {3, 1}}, 1); // smlal: -1
    CHECK(c->r(0) == 0xFFFFFFFFu && c->r(1) == 0xFFFFFFFFu);
    c = run({0xFB01, 0x3002}, {{1, 3}, {2, 4}, {3, 5}}, 1); // mla r0, r1, r2, r3
    CHECK(c->r(0) == 17);
    c = run({0xFB01, 0x3012}, {{1, 3}, {2, 4}, {3, 5}}, 1); // mls
    CHECK(c->r(0) == 0xFFFFFFF9u);
    c = run({0xFBB1, 0xF0F2}, {{1, 7}, {2, 2}}, 1); // udiv
    CHECK(c->r(0) == 3);
    c = run({0xFBB1, 0xF0F2}, {{1, 7}, {2, 0}}, 1); // divide by zero without DIV_0_TRP: 0
    CHECK(c->r(0) == 0 && c->cpu.active_exception() == 0);
    c = run({0xFB91, 0xF0F2}, {{1, 0xFFFFFFF9}, {2, 2}}, 1); // sdiv -7 / 2 = -3 (toward zero)
    CHECK(c->r(0) == 0xFFFFFFFDu);
    c = run({0xFB91, 0xF0F2}, {{1, 0x80000000}, {2, 0xFFFFFFFF}}, 1); // overflow case
    CHECK(c->r(0) == 0x80000000u);
}

TEST_CASE("bit and byte manipulation, saturation and bitfields")
{
    CHECK(run({0xFAB1, 0xF081}, {{1, 0x00010000}}, 1)->r(0) == 15); // clz
    CHECK(run({0xFAB1, 0xF081}, {{1, 0}}, 1)->r(0) == 32);
    CHECK(run({0xFA91, 0xF0A1}, {{1, 1}}, 1)->r(0) == 0x80000000u); // rbit
    CHECK(run({0xBA08}, {{1, 0x12345678}}, 1)->r(0) == 0x78563412u); // rev
    CHECK(run({0xBA48}, {{1, 0x12345678}}, 1)->r(0) == 0x34127856u); // rev16
    CHECK(run({0xBAC8}, {{1, 0x000012F0}}, 1)->r(0) == 0xFFFFF012u); // revsh
    CHECK(run({0xB248}, {{1, 0x80}}, 1)->r(0) == 0xFFFFFF80u); // sxtb
    CHECK(run({0xFA1F, 0xF091}, {{1, 0x12345678}}, 1)->r(0) == 0x3456u); // uxth.w r0, r1, ror #8
    auto c = run({0xF301, 0x0007}, {{1, 300}}, 1); // ssat r0, #8, r1
    CHECK(c->r(0) == 127 && c->flag_q());
    c = run({0xF301, 0x0007}, {{1, static_cast<std::uint32_t>(-300)}}, 1);
    CHECK(c->r(0) == 0xFFFFFF80u && c->flag_q());
    c = run({0xF301, 0x0007}, {{1, 5}}, 1);
    CHECK(c->r(0) == 5 && !c->flag_q());
    c = run({0xF3A1, 0x1008}, {{1, 0x1000}}, 1); // usat r0, #8, r1, asr #4: 256 -> 255
    CHECK(c->r(0) == 255 && c->flag_q());
    c = run({0xF3A1, 0x1008}, {{1, static_cast<std::uint32_t>(-16)}}, 1); // -1 -> 0
    CHECK(c->r(0) == 0 && c->flag_q());
    CHECK(run({0xF341, 0x1007}, {{1, 0x00000F80}}, 1)->r(0) == 0xFFFFFFF8u); // sbfx r0, r1, #4, #8
    CHECK(run({0xF3C1, 0x7003}, {{1, 0xA0000000}}, 1)->r(0) == 0xAu);        // ubfx r0, r1, #28, #4
    CHECK(run({0xF361, 0x200B}, {{0, 0xFFFFFFFF}, {1, 5}}, 1)->r(0) == 0xFFFFF5FFu); // bfi r0, r1, #8, #4
    CHECK(run({0xF36F, 0x000F}, {{0, 0x12345678}}, 1)->r(0) == 0x12340000u);         // bfc r0, #0, #16
    CHECK(run({0xF241, 0x2034, 0xF6CA, 0x30CD}, {}, 2)->r(0) == 0xABCD1234u);        // movw, movt
}

TEST_CASE("loads and stores with every addressing mode")
{
    auto with_memory = [](std::initializer_list<std::uint16_t> code, std::initializer_list<std::pair<int, std::uint32_t>> regs,
                           std::initializer_list<std::pair<std::uint32_t, std::uint32_t>> words, int n) {
        std::vector<std::uint16_t> all(code);
        auto core = std::make_unique<Core>(std::initializer_list<std::uint16_t>{});
        core->bus.load(code_base, reinterpret_cast<const std::uint8_t*>(all.data()), static_cast<std::uint32_t>(all.size() * 2));
        core->cpu.reset();
        for (const auto& [a, v] : words) {
            core->poke(a, v);
        }
        for (const auto& [r, v] : regs) {
            core->set(r, v);
        }
        core->steps(n);
        return core;
    };
    auto k = with_memory({0xF851, 0x0F04}, {{1, ram}}, {{ram + 4, 0xCAFEBABE}}, 1); // ldr r0, [r1, #4]!
    CHECK(k->r(0) == 0xCAFEBABEu && k->r(1) == ram + 4);
    k = with_memory({0xF851, 0x0904}, {{1, ram}}, {{ram, 0x11}}, 1); // ldr r0, [r1], #-4
    CHECK(k->r(0) == 0x11 && k->r(1) == ram - 4);
    k = with_memory({0xF911, 0x0022}, {{1, ram}, {2, 1}}, {{ram + 4, 0x80}}, 1); // ldrsb.w r0, [r1, r2, lsl #2]
    CHECK(k->r(0) == 0xFFFFFF80u);
    k = with_memory({0xF9B1, 0x0002}, {{1, ram}}, {{ram, 0x80010000}}, 1); // ldrsh.w r0, [r1, #2]
    CHECK(k->r(0) == 0xFFFF8001u);
    k = with_memory({0xF821, 0x0C02}, {{0, 0x12345678}, {1, ram + 2}}, {{ram, 0}}, 1); // strh.w r0, [r1, #-2]
    CHECK(k->word(ram) == 0x5678u);
    k = with_memory({0xE9D1, 0x2302}, {{1, ram}}, {{ram + 8, 1}, {ram + 12, 2}}, 1); // ldrd r2, r3, [r1, #8]
    CHECK(k->r(2) == 1 && k->r(3) == 2);
    k = with_memory({0xE961, 0x2302}, {{1, ram + 16}, {2, 7}, {3, 9}}, {}, 1); // strd r2, r3, [r1, #-8]!
    CHECK(k->r(1) == ram + 8 && k->word(ram + 8) == 7 && k->word(ram + 12) == 9);
    k = with_memory({0xC91C}, {{1, ram}}, {{ram, 1}, {ram + 4, 2}, {ram + 8, 3}}, 1); // ldmia r1!, {r2-r4}
    CHECK(k->r(2) == 1 && k->r(3) == 2 && k->r(4) == 3 && k->r(1) == ram + 12);
    k = with_memory({0xE921, 0x000C}, {{1, ram + 8}, {2, 5}, {3, 6}}, {}, 1); // stmdb r1!, {r2, r3}
    CHECK(k->r(1) == ram && k->word(ram) == 5 && k->word(ram + 4) == 6);
    // push {r4, lr}; pop {r4, pc}: returns to LR.
    k = with_memory({0xB510, 0xBD10}, {{4, 44}, {14, 0x08000301}}, {}, 2);
    CHECK(k->r(4) == 44 && k->cpu.pc() == 0x08000300u && k->r(13) == stack_top);
    // ldr r0, [pc, #4] at 0x08000100: Align(PC + 4, 4) + 4 = 0x08000108.
    k = with_memory({0x4801, 0xBF00, 0xBF00, 0xBF00, 0x5678, 0x1234}, {}, {}, 1);
    CHECK(k->r(0) == 0x12345678u);
}

TEST_CASE("IT blocks: conditions, flag-setting 16-bit instructions and skipped instructions")
{
    // ite eq; movs r0, #1; movs r0, #2 (no flags inside the block)
    auto c = run({0xBF0C, 0x2001, 0x2002}, {{0, 9}}, 3, Z);
    CHECK(c->r(0) == 1 && c->flag_z());
    c = run({0xBF0C, 0x2001, 0x2002}, {{0, 9}}, 3, 0);
    CHECK(c->r(0) == 2 && !c->flag_z());
    // itt ne; adds r0, #1; adds r0, #1
    c = run({0xBF1C, 0x3001, 0x3001}, {{0, 5}}, 3, 0);
    CHECK(c->r(0) == 7);
    c = run({0xBF1C, 0x3001, 0x3001}, {{0, 5}}, 3, Z);
    CHECK(c->r(0) == 5);
    // Outside an IT block the same 16-bit adds sets flags: 0xFFFFFFFF + 1.
    c = run({0x3001}, {{0, 0xFFFFFFFF}}, 1);
    CHECK(c->r(0) == 0 && c->flag_z() && c->flag_c());
}

TEST_CASE("branches: B, BL, conditional, CBZ, table branch, ADD PC")
{
    auto c = run({0xF000, 0xFFFE}, {}, 1); // bl .+0x1000 at 0x08000104 (after the MSR prefix)
    CHECK(c->cpu.pc() == 0x08001104u && c->r(14) == 0x08000109u);
    c = run({0xF7FF, 0xBF7E}, {}, 1); // b.w .-0x100
    CHECK(c->cpu.pc() == 0x08000004u);
    c = run({0xF000, 0x80FE}, {}, 1, Z); // beq.w .+0x200
    CHECK(c->cpu.pc() == 0x08000304u);
    c = run({0xF000, 0x80FE}, {}, 1, 0);
    CHECK(c->cpu.pc() == 0x08000108u);
    c = run({0xB108}, {{0, 0}}, 1); // cbz r0, .+6
    CHECK(c->cpu.pc() == 0x0800010Au);
    c = run({0xB108}, {{0, 1}}, 1);
    CHECK(c->cpu.pc() == 0x08000106u);
    c = run({0x4487}, {{0, 0x10}}, 1); // add pc, r0: PC + 4 + 0x10
    CHECK(c->cpu.pc() == 0x08000118u);
    // tbb [r1, r2] with the byte table in RAM: entry 3 -> PC + 4 + 6.
    Core t{{0xE8D1, 0xF002}};
    t.poke(ram, 0x03020100);
    t.set(1, ram);
    t.set(2, 3);
    t.steps(1);
    CHECK(t.cpu.pc() == 0x0800010Au);
    // tbh [r1, r2, lsl #1]: halfword entry 1 = 0x20 -> PC + 4 + 0x40.
    Core h{{0xE8D1, 0xF012}};
    h.poke(ram, 0x00200010);
    h.set(1, ram);
    h.set(2, 1);
    h.steps(1);
    CHECK(h.cpu.pc() == 0x08000144u);
    // adr r0, .+12 at 0x08000104: Align(0x08000108, 4) + 8; subw r0, pc, #14.
    CHECK(run({0xA002}, {}, 1)->r(0) == 0x08000110u);
    CHECK(run({0xF2AF, 0x000E}, {}, 1)->r(0) == 0x08000108u - 14u);
}

TEST_CASE("special registers, exclusive monitor and SVC entry")
{
    auto c = run({0xF3EF, 0x8000}, {}, 1, N | C); // mrs r0, apsr
    CHECK(c->r(0) == (N | C));
    c = run({0xB672, 0xF3EF, 0x8010}, {}, 2); // cpsid i; mrs r0, primask
    CHECK(c->r(0) == 1 && c->cpu.primask() == 1);
    // ldrex r0, [r1, #4]; strex r2, r0, [r1, #4]; strex r2, r0, [r1, #4]
    Core x{{0xE851, 0x0F01, 0xE841, 0x0201, 0xE841, 0x0301}};
    x.poke(ram + 4, 77);
    x.set(1, ram);
    x.steps(2);
    CHECK(x.r(0) == 77 && x.r(2) == 0);
    x.steps(1);
    CHECK(x.r(3) == 1); // monitor cleared by the first STREX
    // svc #1: stacks 8 words on MSP and enters the handler with EXC_RETURN 0xFFFFFFF9.
    Core s{{0xDF01, 0xBF00}};
    s.set(0, 0xA0);
    s.steps(2);
    CHECK(s.cpu.pc() == svc_handler);
    CHECK(s.cpu.active_exception() == 11);
    CHECK(s.r(14) == 0xFFFFFFF9u);
    CHECK(s.r(13) == stack_top - 32);
    CHECK(s.word(stack_top - 32) == 0xA0);           // stacked R0
    CHECK(s.word(stack_top - 8) == code_base + 2);    // return address: after the SVC
    CHECK((s.word(stack_top - 4) & (1u << 24)) != 0); // stacked xPSR with T set
}

TEST_CASE("faults: undefined instruction and BKPT escalate to HardFault; bad reads set BFAR")
{
    Core u{{0xDE00}}; // udf #0 with UsageFault disabled
    u.steps(2);
    CHECK(u.cpu.pc() == hardfault_handler);
    std::uint32_t cfsr = 0;
    std::uint32_t hfsr = 0;
    u.bus.read(0xE000ED28, 4, cfsr);
    u.bus.read(0xE000ED2C, 4, hfsr);
    CHECK((cfsr & (1u << 16)) != 0);  // UNDEFINSTR
    CHECK((hfsr & (1u << 30)) != 0);  // FORCED
    CHECK(u.word(stack_top - 8) == code_base); // return address: the faulting instruction

    // With UsageFault enabled (SHCSR.USGFAULTENA) the UsageFault handler runs.
    Core e{{0xDE00}};
    e.bus.write(0xE000ED24, 4, 1u << 18);
    e.steps(2);
    CHECK(e.cpu.pc() == usage_handler);

    // ldr r0, [r1] from an address with nothing mapped.
    Core b{{0x6808}};
    b.set(1, 0x60000000);
    b.steps(2);
    CHECK(b.cpu.pc() == hardfault_handler);
    std::uint32_t bfar = 0;
    b.bus.read(0xE000ED38, 4, bfar);
    CHECK(bfar == 0x60000000u);
    CHECK(b.cpu.last_fault().find("0x60000000") != std::string::npos);

    // BKPT without a debugger: HardFault with HFSR.DEBUGEVT.
    Core k{{0xBE00}};
    k.steps(2);
    CHECK(k.cpu.pc() == hardfault_handler);
    k.bus.read(0xE000ED2C, 4, hfsr);
    CHECK((hfsr & (1u << 31)) != 0);

    // A fault inside the HardFault handler locks the core up.
    Core l{{0x6808}};
    l.set(1, 0x60000000);
    const std::uint16_t bad_load = 0x6808;
    l.bus.load(hardfault_handler, reinterpret_cast<const std::uint8_t*>(&bad_load), 2);
    l.steps(4);
    CHECK(l.cpu.state() == CoreState::Lockup);
}

TEST_CASE("SysTick interrupts every RVR + 1 cycles and returns to the interrupted code")
{
    // ldr r1, =0xE000E010 (from a literal); movs r0, #99; str r0, [r1, #4]; movs r0, #7; str r0, [r1]; b .
    Core core{{0x4904, 0x2063, 0x6048, 0x2007, 0x6008, 0xE7FE, 0xBF00, 0xBF00, 0xBF00, 0xBF00, 0xE010, 0xE000}};
    const std::uint16_t handler[] = {0x3401, 0x4770}; // adds r4, #1; bx lr
    core.bus.load(systick_handler, reinterpret_cast<const std::uint8_t*>(handler), sizeof handler);
    core.steps(5); // SysTick enabled after the 5th instruction, at cycle 5
    CHECK(core.r(4) == 0);
    core.cpu.run(5 + 100 * 3 + 50);
    // Interrupts at cycles 105, 205, 305 (RVR + 1 = 100 cycles apart).
    CHECK(core.r(4) == 3);
    CHECK(core.cpu.active_exception() == 0);
    CHECK(core.cpu.pc() == code_base + 10); // back in the b . loop
}

TEST_CASE("the STM32F103 runs the blink example with exact SysTick periods")
{
    std::string error;
    const auto image = read_elf(examples / "firmware" / "blink" / "blink.elf", error);
    REQUIRE(image);
    Stm32F103 m;
    REQUIRE(m.load(*image, error));
    m.run_until_ns(2100000000ull);
    std::vector<PinEvent> pc13;
    int pa0 = 0;
    for (const auto& e : m.take_pin_events()) {
        if (e.port == 2 && e.pin == 13) {
            pc13.push_back(e);
        } else if (e.port == 0 && e.pin == 0) {
            ++pa0;
        }
    }
    REQUIRE(pc13.size() == 6); // configured low, driven high, then 4 toggles
    for (std::size_t i = 3; i < pc13.size(); ++i) {
        CHECK(pc13[i].time_ns - pc13[i - 1].time_ns == 500000000ull); // 500 000 ticks of 8000 cycles: exact
    }
    CHECK(pa0 >= 8);
    CHECK(m.take_diagnostics().empty());
}

TEST_CASE("the STM32F103 runs the STM32CubeIDE HAL example at 72 MHz")
{
    std::string error;
    const auto image = read_elf(examples / "firmware" / "hal_clock" / "hal_clock.elf", error);
    REQUIRE(image);
    Stm32F103 m;
    REQUIRE(m.load(*image, error));
    m.run_until_ns(1100000000ull);
    std::string text;
    for (const auto& b : m.take_uart_bytes()) {
        text += static_cast<char>(b.value);
    }
    CHECK(text.find("SYSCLK 72000000 Hz\r\n") != std::string::npos);
    CHECK(text.find("PCLK1  36000000 Hz\r\n") != std::string::npos);
    CHECK(m.hclk_hz() == 72000000u);
    std::uint64_t rise = 0;
    std::vector<std::uint64_t> pulses;
    std::vector<std::uint64_t> led;
    bool pa1 = false;
    bool pc13 = false;
    for (const auto& e : m.take_pin_events()) { // level transitions only (events also report configuration)
        if (e.port == 0 && e.pin == 1 && e.level != pa1) {
            pa1 = e.level;
            if (e.level) {
                rise = e.time_ns;
            } else if (rise) {
                pulses.push_back(e.time_ns - rise);
            }
        } else if (e.port == 2 && e.pin == 13 && e.level != pc13) {
            pc13 = e.level;
            led.push_back(e.time_ns);
        }
    }
    REQUIRE(pulses.size() == 2);
    for (const auto p : pulses) {
        // 72 000 loop instructions plus the GPIO calls around them, at 72 MHz.
        CHECK(p >= 1000000 && p <= 1001000);
    }
    REQUIRE(led.size() == 3);
    CHECK(led[2] - led[1] == 500000000ull);
}

TEST_CASE("EXTI1 from a falling edge on PA1 runs the button handler")
{
    std::string error;
    const auto image = read_elf(examples / "firmware" / "button_irq" / "button_irq.elf", error);
    REQUIRE(image);
    Stm32F103 m;
    REQUIRE(m.load(*image, error));
    m.set_pin_input(0, 1, true);
    m.run_until_ns(100000000);
    m.take_pin_events();
    m.set_pin_input(0, 1, false); // press
    m.run_until_ns(150000000);
    auto events = m.take_pin_events();
    bool led_on = false;
    for (const auto& e : events) {
        if (e.port == 0 && e.pin == 0 && e.level) {
            led_on = true;
            CHECK(e.time_ns - 100000000 < 10000); // the handler runs within 10 us
        }
    }
    CHECK(led_on);
}

TEST_CASE("every 16-bit pattern and a million 32-bit patterns decode and execute without crashing the host")
{
    int bad = 0;
    for (std::uint32_t h = 0; h < 0x10000; ++h) {
        const Insn in = Cpu::decode(h, 0);
        const bool wide = (h >> 11) >= 0x1D;
        bad += (in.op == 0 || in.op >= Cpu::op_count() || in.size != (wide ? 4 : 2)) ? 1 : 0;
    }
    std::uint32_t seed = 12345;
    auto next = [&seed] {
        seed = seed * 1664525u + 1013904223u;
        return seed;
    };
    for (int i = 0; i < 1000000; ++i) {
        const std::uint32_t h1 = 0xE800u | ((next() >> 16) & 0x17FFu);
        const Insn in = Cpu::decode(h1, next() >> 16);
        bad += (in.op == 0 || in.op >= Cpu::op_count() || in.size != 4) ? 1 : 0;
    }
    CHECK(bad == 0);
    // Execute random halfword streams: faults, lockups and wild branches are fine,
    // host crashes or hangs are not.
    for (int run = 0; run < 200; ++run) {
        Core c{{}};
        std::vector<std::uint16_t> code(512);
        for (auto& h : code) {
            h = static_cast<std::uint16_t>(next() >> 16);
        }
        c.bus.load(code_base, reinterpret_cast<const std::uint8_t*>(code.data()), static_cast<std::uint32_t>(code.size() * 2));
        c.cpu.reset();
        for (int n = 0; n < 16; ++n) {
            c.set(n == 13 ? 0 : n, next()); // R13 keeps its value; others random
        }
        c.set(13, stack_top);
        c.steps(5000);
        CHECK(c.cpu.cycles() <= 5000 + 1);
    }
}

namespace {

std::unique_ptr<Stm32F103> machine_with(const char* firmware)
{
    std::string error;
    const auto image = read_elf(examples / "firmware" / firmware / (std::string(firmware) + ".elf"), error);
    if (!image) {
        return nullptr;
    }
    auto m = std::make_unique<Stm32F103>();
    if (!m->load(*image, error)) {
        return nullptr;
    }
    return m;
}

std::uint32_t read_word(Stm32F103& m, std::uint32_t address)
{
    std::uint32_t value = 0;
    m.bus().read(address, 4, value);
    return value;
}

} // namespace

TEST_CASE("resets keep SRAM, record their cause in RCC_CSR and restart the firmware")
{
    auto m = machine_with("blink");
    REQUIRE(m);
    constexpr std::uint32_t rcc_csr = 0x40021024;
    constexpr std::uint32_t marker = 0x20004000; // beyond the blink firmware's data
    CHECK(read_word(*m, rcc_csr) == 0x0C000000u); // power-on: PORRSTF | PINRSTF
    m->run_until_ns(10000000);
    m->bus().write(marker, 4, 0xCAFEF00Du);
    m->bus().write(rcc_csr, 4, 1u << 24); // RMVF clears the flags
    CHECK(read_word(*m, rcc_csr) == 0);

    m->reset(ResetKind::Pin); // NRST
    CHECK(read_word(*m, rcc_csr) == 0x04000000u);
    CHECK(read_word(*m, marker) == 0xCAFEF00Du);
    CHECK(m->cpu().pc() == 0x0800009Cu); // Reset_Handler again

    // SYSRESETREQ from the firmware's point of view: written to AIRCR while running.
    m->run_until_ns(20000000);
    m->bus().write(0xE000ED0C, 4, 0x05FA0004u);
    m->take_pin_events();
    const std::uint64_t reset_at = m->time_ns();
    m->run_until_ns(reset_at + 600000000);
    CHECK(read_word(*m, rcc_csr) == 0x14000000u); // SFTRSTF | PINRSTF
    CHECK(read_word(*m, marker) == 0xCAFEF00Du);
    std::vector<PinEvent> led;
    for (const auto& e : m->take_pin_events()) {
        if (e.port == 2 && e.pin == 13) {
            led.push_back(e);
        }
    }
    REQUIRE(!led.empty());
    // After the reset the firmware starts over: PC13 low 500 ms after it.
    CHECK(!led.back().level);
    CHECK(led.back().time_ns - reset_at > 499000000 && led.back().time_ns - reset_at < 501000000);

    // Power-on clears SRAM and the flags start again.
    m->reset(ResetKind::PowerOn);
    CHECK(read_word(*m, marker) == 0);
    CHECK(read_word(*m, rcc_csr) == 0x0C000000u);
}

TEST_CASE("two runs with the same inputs give identical traces")
{
    auto trace = [](const char* firmware) {
        auto m = machine_with(firmware);
        std::string text;
        if (!m) {
            return text;
        }
        m->set_pin_input(0, 1, true);
        m->run_until_ns(50000000);
        m->set_pin_input(0, 1, false);
        m->uart_receive(1, '1');
        m->run_until_ns(300000000);
        char line[64];
        for (const auto& e : m->take_pin_events()) {
            std::snprintf(line, sizeof line, "%llu P%d.%d=%d\n", static_cast<unsigned long long>(e.time_ns), e.port, e.pin,
                e.level ? 1 : 0);
            text += line;
        }
        for (const auto& b : m->take_uart_bytes()) {
            std::snprintf(line, sizeof line, "%llu U%d=%u\n", static_cast<unsigned long long>(b.time_ns), b.usart, b.value);
            text += line;
        }
        return text;
    };
    for (const char* firmware : {"hal_clock", "lesson2_button", "uart_echo"}) {
        const std::string first = trace(firmware);
        CHECK(!first.empty());
        CHECK(first == trace(firmware));
    }
}

TEST_CASE("a fault and a lockup are reported with the PC and the function name")
{
    // Reset_Handler: ldr r1, [pc, #0]; ldr r0, [r1]; b . ; literal 0x60000000 (nothing mapped).
    // HardFault handler: b . (first image) or the same faulting load (second image).
    auto image_with = [](std::uint16_t hardfault_code) {
        ElfImage image;
        std::vector<std::uint8_t> bytes(0x40, 0);
        auto put32 = [&bytes](std::size_t at, std::uint32_t v) {
            for (int i = 0; i < 4; ++i) {
                bytes[at + static_cast<std::size_t>(i)] = static_cast<std::uint8_t>(v >> (8 * i));
            }
        };
        auto put16 = [&bytes](std::size_t at, std::uint16_t v) {
            bytes[at] = static_cast<std::uint8_t>(v);
            bytes[at + 1] = static_cast<std::uint8_t>(v >> 8);
        };
        put32(0, 0x20005000u);
        put32(4, 0x08000021u);
        put32(12, 0x08000031u);
        put16(0x20, 0x4900); // ldr r1, [pc, #0]  -> literal at 0x24
        put16(0x22, 0x6808); // ldr r0, [r1]
        put32(0x24, 0x60000000u);
        put16(0x30, hardfault_code);
        put16(0x32, 0xE7FE);
        image.entry = 0x08000021;
        image.segments.push_back({0x08000000, 0x08000000, bytes, static_cast<std::uint32_t>(bytes.size()), true});
        image.functions = {{"Reset_Handler", 0x08000020, 8}, {"HardFault_Handler", 0x08000030, 4}};
        return image;
    };
    std::string error;
    Stm32F103 m;
    REQUIRE(m.load(image_with(0xE7FE), error));
    m.run_until_ns(1000000);
    auto diagnostics = m.take_diagnostics();
    REQUIRE(diagnostics.size() == 1);
    CHECK(diagnostics[0].text.find("HardFault") != std::string::npos);
    CHECK(diagnostics[0].text.find("0x60000000") != std::string::npos);
    CHECK(diagnostics[0].text.find("PC 0x08000022") != std::string::npos);
    CHECK(diagnostics[0].text.find("Reset_Handler") != std::string::npos);
    CHECK(m.cpu().pc() == 0x08000030u); // the handler is a b . loop

    Stm32F103 l;
    REQUIRE(l.load(image_with(0x6808), error)); // the handler faults again: lockup
    l.run_until_ns(1000000);
    CHECK(l.cpu().state() == CoreState::Lockup);
    diagnostics = l.take_diagnostics();
    REQUIRE(!diagnostics.empty());
    CHECK(diagnostics.back().text.find("Lockup") != std::string::npos);
    CHECK(diagnostics.back().text.find("HardFault_Handler") != std::string::npos);
    CHECK(l.time_ns() >= 1000000); // time still advances; the host does not hang
}

TEST_CASE("exception model scenarios log the order the ARMv7-M manual prescribes")
{
    // tests/experiments/emu-exceptions/exceptions.s; expected logs from B1.5.
    std::string error;
    const auto image = read_elf(examples.parent_path() / "tests" / "experiments" / "emu-exceptions" / "exceptions.elf", error);
    REQUIRE(image);
    auto run_scenario = [&image](std::uint32_t scenario) {
        Bus bus;
        bus.add_memory("SRAM", 0x20000000, 0x5000, true);
        bus.add_memory("flash", 0x08000000, 0x10000, false);
        bus.add_alias("boot alias", 0, "flash");
        Cpu cpu(bus);
        bus.add_device(0xE000E000, 0x1000, &cpu.scs());
        for (const auto& s : image->segments) {
            bus.load(s.physical, s.data.data(), static_cast<std::uint32_t>(s.data.size()));
        }
        bus.write(0x200000F4, 4, scenario);
        cpu.reset();
        std::uint32_t done = 0;
        for (int i = 0; i < 100 && done != 0xD1FF; ++i) {
            cpu.run(cpu.cycles() + 1000);
            bus.read(0x200000F8, 4, done);
        }
        std::vector<std::uint32_t> log;
        std::uint32_t count = 0;
        bus.read(0x200000F0, 4, count);
        for (std::uint32_t i = 0; i < count && i < 64; ++i) {
            std::uint32_t v = 0;
            bus.read(0x20000100 + 4 * i, 4, v);
            log.push_back(v);
        }
        return log;
    };
    using Log = std::vector<std::uint32_t>;
    CHECK(run_scenario(0) == Log({0xEE}));
    // Preemption: IRQ1 (0x40) preempts IRQ0 (0x80) as soon as IRQ0 pends it.
    CHECK(run_scenario(1) == Log({0xA0, 0xA1, 0xB1, 0xB0, 0xEE}));
    // PRIGROUP 5: 0x90 and 0x80 share group priority 0x80; IRQ1 waits, then tail-chains.
    CHECK(run_scenario(2) == Log({0xA0, 0xB0, 0xA1, 0xB1, 0xEE}));
    // BASEPRI 0x80 masks priority 0x80 until it is cleared.
    CHECK(run_scenario(3) == Log({0xC0, 0xA0, 0xB0, 0xC1, 0xEE}));
    // Pending after CPSIE: by priority, then by exception number.
    CHECK(run_scenario(4) == Log({0xA3, 0xB3, 0xA2, 0xB2, 0xA4, 0xB4, 0xA5, 0xB5, 0xEE}));
    // Thread on PSP: EXC_RETURN 0xFFFFFFFD, PSP restored.
    CHECK(run_scenario(6) == Log({0xA6, 0xFFFFFFFDu, 0xB6, 0x20003000u, 0xEE}));
    // Invalid EXC_RETURN: UsageFault INVPC, escalated (FORCED) because UsageFault is disabled.
    CHECK(run_scenario(7) == Log({0x5C, 0xFF, 0x00040000u, 0x40000000u}));
    // STKALIGN: SP = 0x20003FFC gives a frame at 0x20003FD8 with xPSR bit 9; SP restored.
    CHECK(run_scenario(8) == Log({0xA7, 0x20003FD8u, 0x200, 0xB7, 0x20003FFCu, 0xEE}));
    // FAULTMASK holds IRQ0 back but not NMI; NMI's return keeps FAULTMASK.
    CHECK(run_scenario(9) == Log({0x4E, 0xC8, 0xA0, 0xB0, 0xC9, 0xEE}));
    // WFI wakes on a pending IRQ masked by PRIMASK without running it; SVC under PRIMASK
    // escalates to HardFault (FORCED).
    CHECK(run_scenario(10) == Log({0xD0, 0xA0, 0xB0, 0xD1, 0xFF, 0, 0x40000000u}));
}

namespace {

// A chip whose firmware only spins (b .): tests drive the peripherals through the bus.
std::unique_ptr<Stm32F103> idle_machine()
{
    ElfImage image;
    std::vector<std::uint8_t> bytes(0x24, 0);
    const std::uint32_t words[2] = {0x20005000u, 0x08000021u};
    std::memcpy(bytes.data(), words, sizeof words);
    bytes[0x20] = 0xFE; // b .
    bytes[0x21] = 0xE7;
    image.entry = 0x08000021;
    image.segments.push_back({0x08000000, 0x08000000, bytes, static_cast<std::uint32_t>(bytes.size()), true});
    auto m = std::make_unique<Stm32F103>();
    std::string error;
    if (!m->load(image, error)) {
        return nullptr;
    }
    m->run_until_ns(1000);
    return m;
}

void poke(Stm32F103& m, std::uint32_t address, std::uint32_t value)
{
    m.bus().write(address, 4, value);
}

// Level changes of one pin as (time, level), starting from low.
std::vector<std::pair<std::uint64_t, bool>> edges(const std::vector<PinEvent>& events, int port, int pin)
{
    std::vector<std::pair<std::uint64_t, bool>> out;
    bool level = false;
    for (const auto& e : events) {
        if (e.port == port && e.pin == pin && e.level != level) {
            level = e.level;
            out.emplace_back(e.time_ns, level);
        }
    }
    return out;
}

constexpr std::uint32_t tim1 = 0x40012C00, tim2 = 0x40000000, tim3 = 0x40000400, tim4 = 0x40000800;
constexpr std::uint32_t gpioa_crl = 0x40010800, gpioc_crl = 0x40011000, afio_mapr = 0x40010004;
constexpr std::uint32_t rcc_cfgr = 0x40021004;

} // namespace

TEST_CASE("timer example: PWM, complementary output, update interrupt and input capture with exact edges")
{
    auto m = machine_with("timer_pwm");
    REQUIRE(m);
    m->run_until_ns(200000000);
    const auto events = m->take_pin_events();
    const auto pa0 = edges(events, 0, 0);
    REQUIRE(pa0.size() >= 36);
    // The first pulse starts when CC1E is set, a few instructions before CEN.
    for (std::size_t i = 4; i + 1 < pa0.size(); i += 2) { // 100 Hz, high for 2.5 ms: exact at 8 MHz
        REQUIRE(pa0[i].second);
        CHECK(pa0[i].first - pa0[i - 2].first == 10000000u);
        CHECK(pa0[i + 1].first - pa0[i].first == 2500000u);
    }
    const auto pa8 = edges(events, 0, 8);
    const auto pb13 = edges(events, 1, 13);
    REQUIRE(pa8.size() >= 36);
    for (std::size_t i = 0; i + 1 < pa8.size(); i += 2) { // PWM 2: high for the last 100 counts
        REQUIRE(pa8[i].second);
        CHECK(pa8[i + 1].first - pa8[i].first == 1000000u);
    }
    // The complement changes at the same instants (no dead time).
    std::size_t matched = 0;
    for (const auto& [t, level] : pa8) {
        for (const auto& [tn, level_n] : pb13) {
            if (tn == t) {
                CHECK(level_n != level);
                ++matched;
            }
        }
    }
    CHECK(matched + 1 >= pa8.size());
    const auto pc13 = edges(events, 2, 13);
    REQUIRE(pc13.size() >= 18);
    for (std::size_t i = 1; i < pc13.size(); ++i) {
        CHECK(pc13[i].first - pc13[i - 1].first == 10000000u); // TIM3 update every 10 ms
    }

    // Input capture: PB6 follows PA0 (100 Hz, measured within the 50 us steps).
    bool pa0_level = m->pin_level(0, 0);
    auto feed = [&](std::uint64_t until, std::uint64_t half_period) {
        for (std::uint64_t t = m->time_ns(); t < until; t += 50000) {
            if (half_period == 0) {
                pa0_level = m->pin_level(0, 0);
            } else {
                pa0_level = (t / half_period) % 2 != 0;
            }
            m->set_pin_input(1, 6, pa0_level);
            m->run_until_ns(t + 50000);
        }
    };
    feed(300000000, 0);
    CHECK(m->pin_level(1, 12)); // period 9.95..10.05 ms
    feed(400000000, 10000000);  // 20 ms period
    CHECK(!m->pin_level(1, 12));
    CHECK(m->take_diagnostics().empty());
}

TEST_CASE("timer: toggle on match, down-counting PWM and the APB divider set exact periods")
{
    auto m = idle_machine();
    REQUIRE(m);
    poke(*m, gpioa_crl, 0x444444AAu); // PA0, PA1: alternate-function push-pull
    poke(*m, tim2 + 0x2C, 99);        // ARR: 100 counts
    poke(*m, tim2 + 0x34, 25);        // CCR1
    poke(*m, tim2 + 0x38, 50);        // CCR2
    poke(*m, tim2 + 0x18, (3u << 12) | (6u << 4)); // OC2M toggle, OC1M PWM 1
    poke(*m, tim2 + 0x20, 0x11);      // CC1E, CC2E
    poke(*m, tim2 + 0x00, (1u << 4) | 1u); // DIR down, CEN
    m->run_until_ns(1000000);
    auto events = m->take_pin_events();
    const auto toggles = edges(events, 0, 1);
    REQUIRE(toggles.size() >= 70);
    for (std::size_t i = 1; i < toggles.size(); ++i) {
        CHECK(toggles[i].first - toggles[i - 1].first == 12500u); // 100 cycles at 8 MHz
    }
    const auto pwm = edges(events, 0, 0);
    REQUIRE(pwm.size() >= 70);
    for (std::size_t i = 2; i + 1 < pwm.size(); i += 2) { // the first pulse starts at CC1E
        REQUIRE(pwm[i].second);
        CHECK(pwm[i + 1].first - pwm[i].first == 3250u); // CNT <= 25 while counting down: 26 counts
    }

    // APB1 / 2 keeps the timer clock at HCLK (x2 multiplier); APB1 / 4 halves it.
    poke(*m, rcc_cfgr, 4u << 8);
    m->run_until_ns(2000000);
    m->take_pin_events();
    m->run_until_ns(3000000);
    auto apb_half = edges(m->take_pin_events(), 0, 1);
    REQUIRE(apb_half.size() > 10);
    CHECK(apb_half[2].first - apb_half[1].first == 12500u);
    poke(*m, rcc_cfgr, 5u << 8);
    m->run_until_ns(4000000);
    m->take_pin_events();
    m->run_until_ns(5000000);
    auto quarter = edges(m->take_pin_events(), 0, 1);
    REQUIRE(quarter.size() > 10);
    CHECK(quarter[2].first - quarter[1].first == 25000u);
    CHECK(m->take_diagnostics().empty());
}

TEST_CASE("timer: one-pulse mode, update request source, preload and remapped pins")
{
    auto m = idle_machine();
    REQUIRE(m);
    // One-pulse: the counter stops at the update event.
    poke(*m, tim3 + 0x2C, 99);
    poke(*m, tim3 + 0x00, (1u << 3) | 1u); // OPM, CEN
    m->run_until_ns(100000);
    CHECK(read_word(*m, tim3 + 0x00) == (1u << 3));
    CHECK(read_word(*m, tim3 + 0x24) == 0);
    CHECK((read_word(*m, tim3 + 0x10) & 1u) == 1u); // UIF
    poke(*m, tim3 + 0x10, 0);
    // UG with URS: shadow registers load without UIF.
    poke(*m, tim3 + 0x00, 1u << 2); // URS
    poke(*m, tim3 + 0x18, (6u << 4) | (1u << 3)); // CH1 PWM 1 with preload
    poke(*m, tim3 + 0x34, 40);
    CHECK(read_word(*m, tim3 + 0x34) == 40);
    poke(*m, tim3 + 0x14, 1); // UG
    CHECK((read_word(*m, tim3 + 0x10) & 1u) == 0u);

    // Full remap: TIM3_CH1 on PC6.
    poke(*m, afio_mapr, 3u << 10);
    poke(*m, gpioc_crl, 0x4A444444u); // PC6 alternate-function push-pull
    poke(*m, tim3 + 0x20, 1);         // CC1E
    poke(*m, tim3 + 0x00, 1);         // CEN
    m->run_until_ns(400000);
    const auto pc6 = edges(m->take_pin_events(), 2, 6);
    REQUIRE(pc6.size() >= 4);
    CHECK(pc6[1].first - pc6[0].first == 5000u); // 40 counts high
    CHECK(pc6[2].first - pc6[0].first == 12500u);
}

TEST_CASE("timer: input capture records the counter at the edge and flags overcapture")
{
    auto m = idle_machine();
    REQUIRE(m);
    poke(*m, tim4 + 0x28, 7);      // PSC: 1 MHz
    poke(*m, tim4 + 0x14, 1);      // UG loads it
    poke(*m, tim4 + 0x18, 1);      // CC1S = 01: TI1 (PB6)
    poke(*m, tim4 + 0x20, 1);      // CC1E, rising edge
    poke(*m, tim4 + 0x10, 0);
    poke(*m, tim4 + 0x00, 1);      // CEN
    const std::uint64_t start = m->time_ns();
    m->run_until_ns(start + 300000);
    m->set_pin_input(1, 6, true);
    CHECK((read_word(*m, tim4 + 0x10) & 2u) == 2u); // CC1IF
    m->run_until_ns(start + 400000);
    m->set_pin_input(1, 6, false); // falling: no capture
    m->run_until_ns(start + 500000);
    m->set_pin_input(1, 6, true);
    CHECK((read_word(*m, tim4 + 0x10) & (1u << 9)) != 0); // CC1OF: CC1IF was still set
    const std::uint32_t captured = read_word(*m, tim4 + 0x34);
    CHECK(captured >= 499);
    CHECK(captured <= 501);
    CHECK((read_word(*m, tim4 + 0x10) & 2u) == 0u); // reading CCR1 cleared CC1IF
}

TEST_CASE("timer features that are not simulated are reported once")
{
    auto m = idle_machine();
    REQUIRE(m);
    poke(*m, tim1 + 0x00, 1u << 5); // CMS: center-aligned
    poke(*m, tim1 + 0x00, 2u << 5);
    poke(*m, tim2 + 0x08, 3);       // SMS: encoder mode 3
    const auto diagnostics = m->take_diagnostics();
    REQUIRE(diagnostics.size() == 2);
    CHECK(diagnostics[0].text.find("TIM1: center-aligned counting is not simulated yet") != std::string::npos);
    CHECK(diagnostics[1].text.find("TIM2: the slave mode controller") != std::string::npos);
}

TEST_CASE("ELF errors are reported in plain words")
{
    std::string error;
    CHECK(!parse_elf({1, 2, 3}, error));
    CHECK(error.find("not an ELF") != std::string::npos);
    CHECK(!read_elf(examples / "missing.elf", error));
    CHECK(error.find("Cannot open") != std::string::npos);
}

int main(int argc, char** argv)
{
    if (argc < 2) {
        std::printf("usage: emu_tests <examples-dir> [filter]\n");
        return 2;
    }
    examples = argv[1];
    return simnodus::test::run_all(argc > 2 ? argv[2] : "");
}
