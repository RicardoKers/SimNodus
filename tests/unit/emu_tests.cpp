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
#include "emu/gdb_server.hpp"
#include "emu/stm32f103.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <initializer_list>
#include <memory>
#include <string>
#include <thread>
#include <tuple>
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
std::unique_ptr<Stm32F103> idle_machine(MachineOptions options = {})
{
    ElfImage image;
    std::vector<std::uint8_t> bytes(0x24, 0);
    const std::uint32_t words[2] = {0x20005000u, 0x08000021u};
    std::memcpy(bytes.data(), words, sizeof words);
    bytes[0x20] = 0xFE; // b .
    bytes[0x21] = 0xE7;
    image.entry = 0x08000021;
    image.segments.push_back({0x08000000, 0x08000000, bytes, static_cast<std::uint32_t>(bytes.size()), true});
    auto m = std::make_unique<Stm32F103>(options);
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
    m->run_until_ns(50000000);
    const auto events = m->take_pin_events();
    const auto pa0 = edges(events, 0, 0);
    REQUIRE(pa0.size() >= 90);
    // The first pulse starts when CC1E is set, a few instructions before CEN.
    for (std::size_t i = 4; i + 1 < pa0.size(); i += 2) { // 1 kHz, high for 250 us: exact at 8 MHz
        REQUIRE(pa0[i].second);
        CHECK(pa0[i].first - pa0[i - 2].first == 1000000u);
        CHECK(pa0[i + 1].first - pa0[i].first == 250000u);
    }
    const auto pa8 = edges(events, 0, 8);
    const auto pb13 = edges(events, 1, 13);
    REQUIRE(pa8.size() >= 90);
    for (std::size_t i = 0; i + 1 < pa8.size(); i += 2) { // PWM 2: high for the last 100 counts
        REQUIRE(pa8[i].second);
        CHECK(pa8[i + 1].first - pa8[i].first == 100000u);
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
    REQUIRE(pc13.size() >= 4);
    for (std::size_t i = 1; i < pc13.size(); ++i) {
        CHECK(pc13[i].first - pc13[i - 1].first == 10000000u); // TIM3 update every 10 ms
    }

    // Input capture: PB6 follows PA0 (1 kHz, measured within the 50 us steps).
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
    feed(70000000, 0);
    CHECK(m->pin_level(1, 12)); // period 950..1050 us
    feed(90000000, 1000000);    // 2 ms period
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

namespace {

constexpr std::uint32_t adc1 = 0x40012400, adc2 = 0x40012800;

// Records which pin the ADC sampled at which time; the voltage is a ramp of
// 1 V per ms on every pin plus 0.1 V per pin number (so pins and times differ).
struct AnalogLog {
    std::vector<std::tuple<int, int, std::uint64_t>> samples;
    double volts(int port, int pin, std::uint64_t ns) const
    {
        return static_cast<double>(ns % 3000000) * 1e-6 + 0.1 * (port * 16 + pin);
    }
};

std::unique_ptr<Stm32F103> adc_machine(AnalogLog& log)
{
    auto m = idle_machine();
    if (m) {
        m->set_analog_source([&log](int port, int pin, std::uint64_t ns) {
            log.samples.emplace_back(port, pin, ns);
            return log.volts(port, pin, ns);
        });
    }
    return m;
}

} // namespace

TEST_CASE("ADC: 12-bit codes over 0..3.3 V")
{
    CHECK(Stm32F1Adc::code_of(0.0) == 0);
    CHECK(Stm32F1Adc::code_of(-0.2) == 0);
    CHECK(Stm32F1Adc::code_of(1.65) == 2048);
    CHECK(Stm32F1Adc::code_of(3.3) == 4095);
    CHECK(Stm32F1Adc::code_of(5.0) == 4095);
    CHECK(Stm32F1Adc::code_of(3.3 / 4096 * 1000) == 1000);
}

TEST_CASE("ADC: calibration, a single conversion and its exact sample and end times")
{
    AnalogLog log;
    auto m = adc_machine(log);
    REQUIRE(m);
    // HSI 8 MHz, APB2 / 1, ADCPRE / 2: one ADC clock is 2 CPU cycles (250 ns).
    poke(*m, adc1 + 0x08, 1);         // ADON: power on
    poke(*m, adc1 + 0x08, 1 | 8);     // RSTCAL with ADON still 1 (as HAL writes it): no conversion
    poke(*m, adc1 + 0x08, 1 | 4);     // CAL
    m->run_until_ns(m->time_ns() + 20000);
    CHECK((read_word(*m, adc1 + 0x08) & 4u) == 4u); // still calibrating at 20 us
    m->run_until_ns(m->time_ns() + 1000);
    CHECK((read_word(*m, adc1 + 0x08) & 4u) == 0); // calibration done (83 ADC clocks = 20.75 us)
    CHECK(log.samples.empty());
    poke(*m, adc1 + 0x34, 3);         // SQ1 = IN3 (PA3)
    poke(*m, adc1 + 0x10, 7u << 9);   // SMP3 = 239.5 cycles
    const std::uint64_t start = m->time_ns();
    poke(*m, adc1 + 0x08, 1);         // ADON again: start
    REQUIRE(log.samples.empty());
    m->run_until_ns(start + 100000);
    REQUIRE(log.samples.size() == 1);
    const auto [port, pin, sampled] = log.samples[0];
    CHECK(port == 0);
    CHECK(pin == 3);
    CHECK(sampled - start == 59875); // 239.5 ADC clocks of 250 ns
    CHECK((read_word(*m, adc1) & 2u) == 2u); // EOC
    const std::uint32_t code = read_word(*m, adc1 + 0x4C);
    CHECK(code == Stm32F1Adc::code_of(log.volts(0, 3, sampled)));
    CHECK((read_word(*m, adc1) & 2u) == 0u); // reading DR cleared EOC
    CHECK(m->take_diagnostics().empty());
}

TEST_CASE("ADC: continuous scan of a regular group with per-channel sample times")
{
    AnalogLog log;
    auto m = adc_machine(log);
    REQUIRE(m);
    poke(*m, adc1 + 0x08, 1);
    poke(*m, adc1 + 0x04, 1u << 8);                    // SCAN
    poke(*m, adc1 + 0x2C, 2u << 20);                   // L = 2: three conversions
    poke(*m, adc1 + 0x34, 0u | (8u << 5) | (17u << 10)); // IN0 (PA0), IN8 (PB0), IN17 (VREFINT)
    poke(*m, adc1 + 0x10, 1u << 24);                   // SMP8 = 7.5 cycles, others 1.5
    poke(*m, adc1 + 0x08, 1 | 2 | (7u << 17) | (1u << 20) | (1u << 23)); // CONT, EXTSEL = SWSTART, EXTTRIG, TSVREFE
    const std::uint64_t start = m->time_ns();
    poke(*m, adc1 + 0x08, 1 | 2 | (7u << 17) | (1u << 20) | (1u << 23) | (1u << 22)); // SWSTART
    m->run_until_ns(start + 24000);
    // Group: 14 + 20 + 14 ADC clocks = 12 us; two groups in 24 us, VREFINT not from the pins.
    REQUIRE(log.samples.size() >= 4);
    CHECK(std::get<0>(log.samples[0]) == 0);
    CHECK(std::get<1>(log.samples[0]) == 0);
    CHECK(std::get<2>(log.samples[0]) - start == 375);   // 1.5 clocks
    CHECK(std::get<0>(log.samples[1]) == 1);
    CHECK(std::get<1>(log.samples[1]) == 0);
    CHECK(std::get<2>(log.samples[1]) - start == 5375);  // 14 clocks, then 7.5
    CHECK(std::get<2>(log.samples[2]) - start == 12375); // next group starts at 12 us
    CHECK((read_word(*m, adc1) & 2u) == 2u);
    CHECK(read_word(*m, adc1 + 0x4C) == Stm32F1Adc::code_of(1.20)); // the last of the group: VREFINT
    poke(*m, adc1 + 0x08, 0); // power down stops the conversions
    const auto count = log.samples.size();
    m->run_until_ns(m->time_ns() + 50000);
    CHECK(log.samples.size() == count);
}

TEST_CASE("ADC: injected group with offsets after the regular group, left alignment and the watchdog")
{
    AnalogLog log;
    auto m = adc_machine(log);
    REQUIRE(m);
    poke(*m, adc2 + 0x08, 1);
    poke(*m, adc2 + 0x04, (1u << 8) | (1u << 10) | (1u << 23) | (1u << 22) | (1u << 6)); // SCAN, JAUTO, AWDEN, JAWDEN, AWDIE
    poke(*m, adc2 + 0x24, 100);                     // HTR: every pin voltage here is above it
    poke(*m, adc2 + 0x34, 1);                       // SQ1 = IN1
    poke(*m, adc2 + 0x38, (1u << 20) | (4u << 10) | (5u << 15)); // JL = 1: JSQ3 = IN4, JSQ4 = IN5
    poke(*m, adc2 + 0x14, 10);                      // JOFR1
    poke(*m, adc2 + 0x18, 20);                      // JOFR2
    poke(*m, adc2 + 0x08, 1 | (1u << 11));          // ALIGN left (changes a bit: no start)
    REQUIRE(log.samples.empty());
    poke(*m, adc2 + 0x08, 1 | (1u << 11));          // ADON again: start
    m->run_until_ns(m->time_ns() + 20000);
    REQUIRE(log.samples.size() == 3);
    CHECK(std::get<1>(log.samples[1]) == 4); // JSQ3 first, as JL = 1 selects
    CHECK(std::get<1>(log.samples[2]) == 5);
    const std::uint32_t sr = read_word(*m, adc2);
    CHECK((sr & 7u) == 7u); // AWD, EOC, JEOC
    const auto code1 = Stm32F1Adc::code_of(log.volts(0, 1, std::get<2>(log.samples[0])));
    const auto code4 = Stm32F1Adc::code_of(log.volts(0, 4, std::get<2>(log.samples[1])));
    const auto code5 = Stm32F1Adc::code_of(log.volts(0, 5, std::get<2>(log.samples[2])));
    CHECK(read_word(*m, adc2 + 0x4C) == code1 << 4);
    CHECK(read_word(*m, adc2 + 0x3C) == (((code4 - 10) << 3) & 0xFFFFu));
    CHECK(read_word(*m, adc2 + 0x40) == (((code5 - 20) << 3) & 0xFFFFu));
}

TEST_CASE("ADC: unsimulated features are reported once")
{
    AnalogLog log;
    auto m = adc_machine(log);
    REQUIRE(m);
    poke(*m, adc1 + 0x08, 1 | (1u << 8));             // DMA on ADC1: simulated, no message
    poke(*m, adc2 + 0x08, 1 | (1u << 8));             // ADC2 has no DMA request
    poke(*m, adc1 + 0x08, 1 | (4u << 17) | (1u << 20)); // EXTSEL = TIM3_TRGO
    poke(*m, adc1 + 0x04, 1u << 11);                   // DISCEN
    const auto diagnostics = m->take_diagnostics();
    REQUIRE(diagnostics.size() == 3);
    CHECK(diagnostics[0].text.find("ADC2: DMA") != std::string::npos);
    CHECK(diagnostics[1].text.find("timer or EXTI trigger") != std::string::npos);
    CHECK(diagnostics[2].text.find("discontinuous") != std::string::npos);
}

namespace {

constexpr std::uint32_t dma1 = 0x40020000;

// Register of DMA1 channel n (1..7): CCR, CNDTR, CPAR, CMAR at 0, 4, 8, 12.
constexpr std::uint32_t dma_ch(int n, std::uint32_t reg)
{
    return dma1 + 0x08 + 0x14 * static_cast<std::uint32_t>(n - 1) + reg;
}

std::uint16_t read_half(Stm32F103& m, std::uint32_t address)
{
    std::uint32_t value = 0;
    m.bus().read(address, 2, value);
    return static_cast<std::uint16_t>(value);
}

} // namespace

TEST_CASE("DMA: ADC1 scan results go to memory in circular mode with half and complete flags")
{
    AnalogLog log;
    auto m = adc_machine(log);
    REQUIRE(m);
    constexpr std::uint32_t buffer = 0x20000100;
    poke(*m, dma_ch(1, 8), adc1 + 0x4C);            // CPAR: ADC1_DR
    poke(*m, dma_ch(1, 12), buffer);                // CMAR
    poke(*m, dma_ch(1, 4), 4);                      // CNDTR: two groups of two
    // PSIZE 16, MSIZE 16, MINC, CIRC, HTIE, TCIE, EN
    poke(*m, dma_ch(1, 0), (1u << 8) | (1u << 10) | (1u << 7) | (1u << 5) | (1u << 2) | (1u << 1) | 1u);
    poke(*m, adc1 + 0x08, 1);
    poke(*m, adc1 + 0x04, 1u << 8);                 // SCAN
    poke(*m, adc1 + 0x2C, 1u << 20);                // L = 1: two conversions
    poke(*m, adc1 + 0x34, 2u | (3u << 5));          // IN2, IN3
    poke(*m, adc1 + 0x08, 1 | 2 | (1u << 8));       // CONT, DMA
    const std::uint64_t start = m->time_ns();
    poke(*m, adc1 + 0x08, 1 | 2 | (1u << 8));       // ADON again: start
    m->run_until_ns(start + 7000);                  // one group: 2 x 14 ADC clocks = 7 us
    REQUIRE(log.samples.size() == 2);
    CHECK((read_word(*m, dma1) & 0x7u) == 0x5u);    // GIF1, HTIF1
    CHECK(read_word(*m, dma_ch(1, 4)) == 2);
    m->run_until_ns(start + 14000);
    REQUIRE(log.samples.size() == 4);
    CHECK((read_word(*m, dma1) & 0x7u) == 0x7u);    // and TCIF1
    CHECK(read_word(*m, dma_ch(1, 4)) == 4);        // circular: reloaded
    for (std::size_t i = 0; i < 4; ++i) {
        const auto [port, pin, at] = log.samples[i];
        CHECK(read_half(*m, buffer + 2 * static_cast<std::uint32_t>(i)) == Stm32F1Adc::code_of(log.volts(port, pin, at)));
    }
    CHECK((read_word(*m, adc1) & 2u) == 0u); // the DMA read DR, which clears EOC
    poke(*m, dma1 + 4, 1u);                  // CGIF1 clears the channel's flags
    CHECK((read_word(*m, dma1) & 0xFu) == 0u);
    m->run_until_ns(start + 21000);          // circular: the buffer is written again from its start
    const auto [port, pin, at] = log.samples[4];
    CHECK(read_half(*m, buffer) == Stm32F1Adc::code_of(log.volts(port, pin, at)));
    CHECK(m->take_diagnostics().empty());
}

TEST_CASE("DMA: USART1 sends a buffer and receives bytes into memory")
{
    auto m = idle_machine();
    REQUIRE(m);
    constexpr std::uint32_t text = 0x20000200;
    constexpr std::uint32_t received = 0x20000300;
    const char hello[] = "Hello";
    for (std::uint32_t i = 0; i < 5; ++i) {
        m->bus().write(text + i, 1, static_cast<std::uint8_t>(hello[i]));
    }
    m->take_uart_bytes();
    poke(*m, 0x4001380C, (1u << 13) | (1u << 3) | (1u << 2)); // UE, TE, RE
    // TX: channel 4, memory to peripheral, 8-bit, MINC; the USART asks once DMAT is set.
    poke(*m, dma_ch(4, 8), 0x40013804);
    poke(*m, dma_ch(4, 12), text);
    poke(*m, dma_ch(4, 4), 5);
    poke(*m, dma_ch(4, 0), (1u << 7) | (1u << 4) | 1u);
    CHECK(m->take_uart_bytes().empty());
    poke(*m, 0x40013814, 1u << 7); // CR3.DMAT
    std::string sent;
    for (const auto& b : m->take_uart_bytes()) {
        sent += static_cast<char>(b.value);
    }
    CHECK(sent == "Hello");
    CHECK(read_word(*m, dma_ch(4, 4)) == 0);
    CHECK((read_word(*m, dma1) >> 12 & 2u) == 2u); // TCIF4
    // RX: channel 5, peripheral to memory, 8-bit, MINC.
    poke(*m, dma_ch(5, 8), 0x40013804);
    poke(*m, dma_ch(5, 12), received);
    poke(*m, dma_ch(5, 4), 3);
    poke(*m, dma_ch(5, 0), (1u << 7) | 1u);
    poke(*m, 0x40013814, (1u << 7) | (1u << 6)); // CR3.DMAR too
    for (const char c : std::string("abcd")) {
        m->uart_receive(1, static_cast<std::uint8_t>(c));
    }
    CHECK((read_word(*m, received) & 0xFFFFFFu) == 0x636261u); // "abc"
    CHECK(read_word(*m, dma_ch(5, 4)) == 0);
    CHECK((read_word(*m, 0x40013800) & 0x20u) == 0x20u); // "d" waits in DR: the channel is done
    CHECK((read_word(*m, 0x40013804) & 0xFFu) == 'd');
}

TEST_CASE("DMA: memory to memory with size conversion, and a transfer error")
{
    auto m = idle_machine();
    REQUIRE(m);
    poke(*m, 0x20000400, 0x11223344);
    poke(*m, 0x20000404, 0x55667788);
    // 32-bit source (peripheral side) to 16-bit destination: truncated.
    poke(*m, dma_ch(2, 8), 0x20000400);
    poke(*m, dma_ch(2, 12), 0x20000500);
    poke(*m, dma_ch(2, 4), 2);
    poke(*m, dma_ch(2, 0), (1u << 14) | (2u << 8) | (1u << 10) | (1u << 6) | (1u << 7) | 1u);
    CHECK(read_word(*m, 0x20000500) == 0x77883344u);
    CHECK((read_word(*m, dma1) >> 4 & 2u) == 2u); // TCIF2
    // A memory address outside the chip: TEIF, the channel switches off.
    poke(*m, dma_ch(3, 8), 0x20000400);
    poke(*m, dma_ch(3, 12), 0x30000000);
    poke(*m, dma_ch(3, 4), 1);
    poke(*m, dma_ch(3, 0), (1u << 14) | 1u);
    CHECK((read_word(*m, dma1) >> 8 & 8u) == 8u); // TEIF3
    CHECK((read_word(*m, dma_ch(3, 0)) & 1u) == 0u);
    const auto diagnostics = m->take_diagnostics();
    REQUIRE(diagnostics.size() == 1);
    CHECK(diagnostics[0].text.find("transfer error") != std::string::npos);
}

TEST_CASE("an STM32CubeIDE HAL project runs ADC1 scan with DMA, TIM2 PWM and UART DMA at 72 MHz")
{
    // examples/firmware/hal_adc_dma: HAL_ADCEx_Calibration_Start, HAL_ADC_Start_DMA
    // (circular), HAL_TIM_PWM_Start, HAL_UART_Transmit_DMA. Without a circuit PA1
    // reads 0 V; VREFINT is 1.20 V.
    auto m = machine_with("hal_adc_dma");
    REQUIRE(m);
    m->set_analog_source([](int port, int pin, std::uint64_t) { return port == 0 && pin == 1 ? 2.0 : 0.0; });
    m->run_until_ns(1100000000);
    std::string text;
    for (const auto& b : m->take_uart_bytes()) {
        text += static_cast<char>(b.value);
    }
    std::printf("%s", text.substr(text.size() > 120 ? text.size() - 120 : 0).c_str());
    const auto at = text.rfind("PA1 ");
    REQUIRE(at != std::string::npos);
    CHECK(std::atoi(text.c_str() + at + 4) == static_cast<int>(Stm32F1Adc::code_of(2.0)));
    CHECK(text.find("VREFINT 1489") != std::string::npos);
    // One group: 2 x (239.5 + 12.5) ADC clocks at 12 MHz = 42 us: 23 809 groups per second.
    const auto groups = std::atoi(text.c_str() + text.rfind("groups ") + 7);
    CHECK(groups >= 23800);
    CHECK(groups <= 23815);
    // TIM2 PWM at 1 kHz with the duty of the PA1 code (2482 / 4096 = 60.5 %).
    std::vector<std::uint64_t> rises;
    std::uint64_t high = 0;
    bool level = false;
    std::uint64_t rose = 0;
    for (const auto& e : m->take_pin_events()) {
        if (e.port == 0 && e.pin == 0 && e.level != level) {
            level = e.level;
            if (level) {
                rises.push_back(e.time_ns);
                rose = e.time_ns;
            } else if (rose) {
                high = e.time_ns - rose;
            }
        }
    }
    REQUIRE(rises.size() > 100);
    CHECK(rises.back() - rises[rises.size() - 2] == 1000000u);
    CHECK(high == 605000u);
    CHECK(m->take_diagnostics().empty());
}

namespace {

// The machine's owner: services the GDB server between 1 ms run steps, as the
// microcontroller runner does.
struct DebugTarget {
    std::unique_ptr<Stm32F103> machine;
    std::unique_ptr<GdbServer> server;
    std::atomic<bool> stop{false};
    std::atomic<std::uint64_t> time_ns{0};
    std::thread thread;

    bool start(const char* firmware)
    {
        machine = machine_with(firmware);
        if (!machine) {
            return false;
        }
        server = std::make_unique<GdbServer>(*machine);
        std::string error;
        if (!server->start(0, error)) {
            return false;
        }
        thread = std::thread([this] {
            while (!stop) {
                if (server->service(5)) {
                    machine->run_until_ns(machine->time_ns() + 1000000);
                    server->after_run();
                    time_ns = machine->time_ns();
                }
            }
        });
        return true;
    }
    ~DebugTarget()
    {
        stop = true;
        if (thread.joinable()) {
            thread.join();
        }
    }
};

std::string ask(GdbSocket& client, const std::string& packet)
{
    std::string reply;
    client.send_raw(GdbSocket::frame(packet));
    if (!client.receive_packet(5000, reply)) {
        return "<no reply>";
    }
    return reply;
}

std::uint32_t le_word(const std::string& hex)
{
    std::uint32_t value = 0;
    for (int i = 3; i >= 0; --i) {
        value = (value << 8) | static_cast<std::uint32_t>(std::stoul(hex.substr(static_cast<std::size_t>(2 * i), 2), nullptr, 16));
    }
    return value;
}

std::uint32_t function_address(const char* firmware, const char* name)
{
    std::string error;
    const auto image = read_elf(examples / "firmware" / firmware / (std::string(firmware) + ".elf"), error);
    if (image) {
        for (const auto& f : image->functions) {
            if (f.name == name) {
                return f.address & ~1u;
            }
        }
    }
    return 0;
}

} // namespace

TEST_CASE("GDB server: attach halts, breakpoints, registers, memory, step, reset and detach")
{
    DebugTarget target;
    REQUIRE(target.start("blink"));
    GdbSocket client;
    std::string error;
    REQUIRE(client.connect_client(target.server->port(), error));

    CHECK(ask(client, "qSupported:multiprocess+;xmlRegisters=arm").find("qXfer:features:read+") != std::string::npos);
    const std::string xml = ask(client, "qXfer:features:read:target.xml:0,fff");
    CHECK(xml.rfind("l<?xml", 0) == 0);
    CHECK(xml.find("org.gnu.gdb.arm.m-profile") != std::string::npos);
    CHECK(ask(client, "?") == "S05");
    const std::string registers = ask(client, "g");
    REQUIRE(registers.size() == 21 * 8);
    const auto reset_handler = function_address("blink", "Reset_Handler");
    REQUIRE(reset_handler != 0);
    const std::uint64_t halted_at = target.time_ns;
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    CHECK(target.time_ns == halted_at); // attaching halted the firmware

    // A breakpoint in the SysTick handler: continue stops there.
    const auto systick = function_address("blink", "SysTick_Handler");
    REQUIRE(systick != 0);
    char z0[40];
    std::snprintf(z0, sizeof z0, "Z0,%x,2", static_cast<unsigned>(systick));
    CHECK(ask(client, z0) == "OK");
    CHECK(ask(client, "c") == "S05");
    CHECK(le_word(ask(client, "pf")) == systick);
    CHECK((le_word(ask(client, "p10")) & 0x1FFu) == 15u); // xPSR: in exception 15 (SysTick)
    // Single step moves on; continue comes back to the breakpoint one tick later.
    CHECK(ask(client, "s") == "S05");
    CHECK(le_word(ask(client, "pf")) != systick);
    CHECK(ask(client, "vCont;c") == "S05");
    CHECK(le_word(ask(client, "pf")) == systick);
    z0[0] = 'z';
    CHECK(ask(client, z0) == "OK");

    // Memory: write SRAM, read it back; registers: write r0.
    CHECK(ask(client, "M20004800,4:78563412") == "OK");
    CHECK(ask(client, "m20004800,4") == "78563412");
    CHECK(ask(client, "P0=efbeadde") == "OK");
    CHECK(le_word(ask(client, "p0")) == 0xDEADBEEFu);
    CHECK(ask(client, "m60000000,4") == "E01"); // nothing there

    // monitor reset: an O packet with text, then OK; the PC is the reset handler.
    std::string text;
    client.send_raw(GdbSocket::frame("qRcmd,7265736574")); // "reset"
    REQUIRE(client.receive_packet(5000, text));
    CHECK(text[0] == 'O');
    REQUIRE(client.receive_packet(5000, text));
    CHECK(text == "OK");
    CHECK(le_word(ask(client, "pf")) == reset_handler);

    // Ctrl-C while running halts with SIGINT.
    client.send_raw(GdbSocket::frame("c"));
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    client.send_raw("\x03");
    CHECK((client.receive_packet(5000, text) ? text : "") == "S02");

    // Detach: the firmware runs on.
    CHECK(ask(client, "D") == "OK");
    const std::uint64_t detached_at = target.time_ns;
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    CHECK(target.time_ns > detached_at);
}

TEST_CASE("GDB server: after a load into flash the next step starts from reset, as with an ST-LINK")
{
    DebugTarget target;
    REQUIRE(target.start("blink"));
    GdbSocket client;
    std::string error;
    REQUIRE(client.connect_client(target.server->port(), error));
    REQUIRE(ask(client, "?") == "S05");
    const auto reset_handler = function_address("blink", "Reset_Handler");
    CHECK(ask(client, "Pf=00010008") == "OK"); // gdb's load sets some PC; the vectors decide
    // Rewrite the first vector word with its own value, as `load` writes the image.
    const std::string sp = ask(client, "m8000000,4");
    CHECK(ask(client, "M8000000,4:" + sp) == "OK");
    CHECK(ask(client, "s") == "S05");
    const auto pc = le_word(ask(client, "pf"));
    CHECK(pc > reset_handler);
    CHECK(pc <= reset_handler + 4); // one instruction of the reset handler
    CHECK(ask(client, "D") == "OK");
}

TEST_CASE("GDB server: a write watchpoint halts after the store and reports the address")
{
    DebugTarget target;
    REQUIRE(target.start("blink"));
    GdbSocket client;
    std::string error;
    REQUIRE(client.connect_client(target.server->port(), error));
    REQUIRE(ask(client, "?") == "S05");
    // blink's variables lie at the start of SRAM; the stack is at its end.
    CHECK(ask(client, "Z2,20000000,100") == "OK");
    CHECK(ask(client, "c") == "T05watch:20000000;");
    CHECK(ask(client, "z2,20000000,100") == "OK");
    CHECK(ask(client, "Z3,20000000,100") == "OK");
    CHECK(ask(client, "c").rfind("T05rwatch:", 0) == 0);
    CHECK(ask(client, "D") == "OK");
}

TEST_CASE("GDB server: BKPT halts with a debugger and faults without one")
{
    ElfImage image;
    std::vector<std::uint8_t> bytes(0x24, 0);
    const std::uint32_t words[2] = {0x20005000u, 0x08000021u};
    std::memcpy(bytes.data(), words, sizeof words);
    bytes[0x20] = 0x00; // bkpt #0
    bytes[0x21] = 0xBE;
    bytes[0x22] = 0xFE; // b .
    bytes[0x23] = 0xE7;
    image.entry = 0x08000021;
    image.segments.push_back({0x08000000, 0x08000000, bytes, static_cast<std::uint32_t>(bytes.size()), true});
    Stm32F103 m;
    std::string error;
    REQUIRE(m.load(image, error));
    m.cpu().set_debugger_attached(true);
    m.run_until_ns(10000);
    CHECK(m.halted());
    CHECK(m.cpu().pc() == 0x08000020u);
    CHECK(m.take_diagnostics().empty());
    m.cpu().set_debugger_attached(false);
    m.run_until_ns(20000);
    const auto diagnostics = m.take_diagnostics();
    REQUIRE(!diagnostics.empty());
    CHECK(diagnostics[0].text.find("BKPT") != std::string::npos);
}

TEST_CASE("USART: frames on the TX pin at the baud rate, TXE and TC follow the shift register")
{
    auto m = idle_machine();
    REQUIRE(m);
    m->take_uart_bytes();
    poke(*m, 0x40010804, 0x444444B4u);        // PA9: alternate-function push-pull
    poke(*m, 0x40013808, 69);                  // BRR: 8 MHz / 69 = 115942 baud; a bit is 69 cycles
    poke(*m, 0x4001380C, (1u << 13) | (1u << 3)); // UE, TE
    m->run_until_ns(m->time_ns() + 10000);
    m->take_pin_events();
    const std::uint64_t start = m->time_ns();
    poke(*m, 0x40013804, 0x55);                // into the shift register: TXE stays set
    CHECK((read_word(*m, 0x40013800) & 0xC0u) == 0x80u); // TXE, not TC
    poke(*m, 0x40013804, 0xF0);                // waits in DR
    CHECK((read_word(*m, 0x40013800) & 0x80u) == 0u);
    m->run_until_ns(start + 200000);
    CHECK((read_word(*m, 0x40013800) & 0xC0u) == 0xC0u); // both frames done
    const auto bytes = m->take_uart_bytes();
    REQUIRE(bytes.size() == 2);
    CHECK(bytes[0].time_ns - start == 10 * 69 * 125u); // 10 bits of 8.625 us
    CHECK(bytes[1].time_ns - start == 20 * 69 * 125u);
    // 0x55: start 0, then 1 0 1 0 1 0 1 0, stop 1: an edge at every bit boundary.
    const auto pa9 = edges(m->take_pin_events(), 0, 9);
    REQUIRE(pa9.size() >= 10);
    // edges() starts from low, so the first change seen is the start of the second
    // bit (high); the start bit's falling edge is at `start`.
    for (std::size_t i = 1; i < 9; ++i) {
        CHECK(pa9[i].first - pa9[i - 1].first == 69 * 125u);
    }
}

TEST_CASE("USART: frames on the RX pin are decoded from the pin's edge times")
{
    // 8 MHz, BRR 69: 115942 baud, a bit is 8.625 us. PA10 stays a floating input.
    constexpr std::uint64_t bit = 69 * 125;
    auto receiver = [](MachineOptions options, std::uint32_t cr1 = 0) {
        auto m = idle_machine(options);
        if (m) {
            m->set_pin_input(0, 10, true); // the line rests high
            poke(*m, 0x40013808, 69);
            poke(*m, 0x4001380C, (1u << 13) | (1u << 2) | cr1); // UE, RE
        }
        return m;
    };
    // Edges of one frame from `start`: start bit, LSB first, stop bit (`stop` level).
    auto frame = [](std::uint64_t start, std::uint32_t bits, int count, bool stop) {
        std::vector<std::pair<std::uint64_t, bool>> edges{{start, false}};
        bool level = false;
        for (int i = 0; i <= count; ++i) {
            const bool next = i < count ? ((bits >> i) & 1u) != 0 : stop;
            if (next != level) {
                edges.emplace_back(start + bit * static_cast<std::uint64_t>(i + 1), next);
                level = next;
            }
        }
        if (!stop) {
            edges.emplace_back(start + bit * static_cast<std::uint64_t>(count + 2), true); // the line goes back up
        }
        return edges;
    };
    // Each edge reaches the chip `late` ns after it happened, with its own time.
    auto deliver = [](Stm32F103& m, const std::vector<std::pair<std::uint64_t, bool>>& edges, std::uint64_t late) {
        for (const auto& [at, level] : edges) {
            m.run_until_ns(std::max(m.time_ns(), at + late));
            m.set_pin_input(0, 10, level, at);
        }
    };
    auto received = [](Stm32F103& m) {
        std::string text;
        while (read_word(m, 0x40013800) & 0x20u) {
            text += static_cast<char>(read_word(m, 0x40013804));
        }
        return text;
    };

    { // in time: "Hi" one after the other
        auto m = receiver({});
        REQUIRE(m);
        const std::uint64_t start = m->time_ns() + 5000;
        deliver(*m, frame(start, 'H', 8, true), 0);
        deliver(*m, frame(start + 10 * bit, 'i', 8, true), 0);
        m->run_until_ns(start + 21 * bit);
        CHECK(received(*m) == "Hi"); // the second byte waits until DR is read
    }
    { // late by 150 us (inputs at window boundaries): decided after the lag, same bytes
        MachineOptions options;
        options.uart_rx_lag_ns = 200000;
        auto m = receiver(options);
        REQUIRE(m);
        const std::uint64_t start = m->time_ns() + 5000;
        deliver(*m, frame(start, 'O', 8, true), 150000);
        m->run_until_ns(start + 9 * bit + bit / 2 + 190000);
        CHECK(!(read_word(*m, 0x40013800) & 0x20u)); // not decided until 200 us after the stop bit's center
        deliver(*m, frame(start + 10 * bit, 'K', 8, true), 150000);
        m->run_until_ns(start + 30 * bit + 200000);
        std::string text = received(*m);
        text += received(*m);
        CHECK(text == "OK");
    }
    { // a low stop bit sets FE; even parity with a wrong bit sets PE
        auto m = receiver({}, 1u << 10); // PCE, even parity: 7 data bits and the parity bit
        REQUIRE(m);
        const std::uint64_t start = m->time_ns() + 5000;
        deliver(*m, frame(start, 0x41u | 0x00u, 8, true), 0); // 'A' has two ones: parity bit 0 is right
        m->run_until_ns(start + 11 * bit);
        CHECK((read_word(*m, 0x40013800) & 0x23u) == 0x20u);
        CHECK(read_word(*m, 0x40013804) == 0x41u);
        deliver(*m, frame(start + 20 * bit, 0x43u, 8, true), 0); // 'C' has three ones: parity 0 is wrong
        m->run_until_ns(start + 31 * bit);
        CHECK((read_word(*m, 0x40013800) & 0x23u) == 0x21u); // RXNE, PE
        read_word(*m, 0x40013804);
        deliver(*m, frame(start + 40 * bit, 0x41u, 8, false), 0);
        m->run_until_ns(start + 52 * bit);
        CHECK((read_word(*m, 0x40013800) & 0x22u) == 0x22u); // RXNE, FE
    }
    { // a short low glitch is not a start bit
        auto m = receiver({});
        REQUIRE(m);
        const std::uint64_t start = m->time_ns() + 5000;
        deliver(*m, {{start, false}, {start + bit / 4, true}}, 0);
        m->run_until_ns(start + 12 * bit);
        CHECK(received(*m).empty());
    }
    { // a UART fed by another board's bytes, or bits too short for the circuit, do not decode the pin
        MachineOptions linked;
        linked.uart_rx_pins = 0x6u;
        MachineOptions coarse;
        coarse.uart_rx_min_bit_ns = 4 * 100000; // four steps of 100 us
        for (const auto& options : {linked, coarse}) {
            auto m = receiver(options);
            REQUIRE(m);
            m->take_diagnostics();
            const std::uint64_t start = m->time_ns() + 5000;
            deliver(*m, frame(start, 'x', 8, true), 0);
            m->run_until_ns(start + 12 * bit);
            CHECK(received(*m).empty());
            const auto diagnostics = m->take_diagnostics();
            CHECK(diagnostics.size() == (options.uart_rx_min_bit_ns > 0 ? 1u : 0u));
            if (!diagnostics.empty()) {
                CHECK(diagnostics.front().text.find("115942 baud") != std::string::npos);
            }
        }
    }
}

// ---------------------------------------------------------------------------
// System peripherals (SN-187): CRC, IWDG, WWDG, RTC and BKP.

namespace {

constexpr std::uint32_t crc_base = 0x40023000, iwdg = 0x40003000, wwdg = 0x40002C00;
constexpr std::uint32_t rtc = 0x40002800, bkp = 0x40006C00, pwr_cr = 0x40007000;
constexpr std::uint32_t rcc_bdcr = 0x40021020, rcc_csr_reg = 0x40021024;
constexpr std::uint32_t exti_imr = 0x40010400, exti_rtsr = 0x40010408, exti_pr = 0x40010414;
constexpr std::uint32_t nvic_ispr0 = 0xE000E200, nvic_ispr1 = 0xE000E204;

bool has_diagnostic(const std::vector<Diagnostic>& list, const char* text)
{
    return std::any_of(list.begin(), list.end(), [&](const Diagnostic& d) { return d.text.find(text) != std::string::npos; });
}

} // namespace

TEST_CASE("CRC: CRC-32 over words as the STM32F1 computes it, IDR and RESET")
{
    auto m = idle_machine();
    REQUIRE(m);
    CHECK(read_word(*m, crc_base) == 0xFFFFFFFFu);
    poke(*m, crc_base, 0x12345678u);
    CHECK(read_word(*m, crc_base) == 0xDF8A8A2Bu); // ST's documented result for this word
    poke(*m, crc_base, 0x9ABCDEF0u);
    CHECK(read_word(*m, crc_base) == Stm32F1Crc::step(0xDF8A8A2Bu, 0x9ABCDEF0u));
    poke(*m, crc_base + 4, 0x1A5u);
    CHECK(read_word(*m, crc_base + 4) == 0xA5u); // 8 bits
    poke(*m, crc_base + 8, 1);                   // RESET
    CHECK(read_word(*m, crc_base) == 0xFFFFFFFFu);
    CHECK(read_word(*m, crc_base + 4) == 0xA5u); // IDR is not reset by RESET
    CHECK(m->take_diagnostics().empty());
}

TEST_CASE("IWDG: a reload keeps the chip running; a missed one resets it with IWDGRSTF")
{
    auto m = idle_machine();
    REQUIRE(m);
    poke(*m, rcc_csr_reg, 1u << 24); // RMVF
    poke(*m, iwdg + 4, 3);           // locked: ignored
    CHECK(read_word(*m, iwdg + 4) == 0);
    poke(*m, iwdg, 0x5555);
    poke(*m, iwdg + 4, 0);  // prescaler / 4
    poke(*m, iwdg + 8, 99); // 100 counts at 10 kHz: 10 ms
    CHECK(read_word(*m, iwdg + 8) == 99u);
    const std::uint64_t start = m->time_ns();
    poke(*m, iwdg, 0xCCCC);
    m->run_until_ns(start + 8000000);
    poke(*m, iwdg, 0xAAAA); // reload at 8 ms: the next timeout is at 18 ms
    m->run_until_ns(start + 17900000);
    CHECK(read_word(*m, rcc_csr_reg) == 0);
    m->run_until_ns(start + 18100000);
    CHECK(read_word(*m, rcc_csr_reg) == 0x24000000u); // IWDGRSTF | PINRSTF
    CHECK(m->cpu().pc() == 0x08000020u);              // the firmware starts over
    CHECK(has_diagnostic(m->take_diagnostics(), "independent watchdog"));
    // A reset stops the watchdog.
    m->run_until_ns(start + 60000000);
    CHECK(read_word(*m, rcc_csr_reg) == 0x24000000u);
}

TEST_CASE("WWDG: counter, early wakeup at 0x40, reset at 0x3F, refresh above the window and T6 cleared")
{
    auto m = idle_machine();
    REQUIRE(m);
    poke(*m, rcc_csr_reg, 1u << 24);
    // HSI 8 MHz, APB1 / 1: one count every 4096 / 8 MHz = 512 us.
    poke(*m, wwdg + 4, 0x200u | 0x50u); // EWI, window 0x50
    const std::uint64_t start = m->time_ns();
    poke(*m, wwdg, 0x80u | 0x7Fu);      // WDGA, T = 0x7F
    m->run_until_ns(start + 1100000);
    CHECK((read_word(*m, wwdg) & 0x7Fu) == 0x7Du); // two counts after 1.1 ms
    m->run_until_ns(start + 32300000);             // 63 counts (32.256 ms): 0x40
    CHECK((read_word(*m, wwdg) & 0x7Fu) == 0x40u);
    CHECK(read_word(*m, wwdg + 8) == 1u);          // EWIF
    CHECK((read_word(*m, nvic_ispr0) & 1u) == 1u); // WWDG interrupt pending
    CHECK(read_word(*m, rcc_csr_reg) == 0);
    m->run_until_ns(start + 32800000);             // 64 counts: T6 clears
    CHECK(read_word(*m, rcc_csr_reg) == 0x44000000u); // WWDGRSTF | PINRSTF
    CHECK(has_diagnostic(m->take_diagnostics(), "window watchdog"));
    CHECK((read_word(*m, wwdg) & 0x80u) == 0);     // the reset cleared WDGA

    // Refreshed above the window: an immediate reset.
    poke(*m, rcc_csr_reg, 1u << 24);
    poke(*m, wwdg + 4, 0x50u);
    poke(*m, wwdg, 0xFFu);
    poke(*m, wwdg, 0x7Fu); // the counter (0x7F) is above the window (0x50)
    m->run_until_ns(m->time_ns() + 1000);
    CHECK(read_word(*m, rcc_csr_reg) == 0x44000000u);
    CHECK(has_diagnostic(m->take_diagnostics(), "too early"));

    // Inside the window the refresh is accepted.
    poke(*m, rcc_csr_reg, 1u << 24);
    poke(*m, wwdg + 4, 0x7Fu);
    poke(*m, wwdg, 0xFFu);
    m->run_until_ns(m->time_ns() + 20000000);
    poke(*m, wwdg, 0x7Fu);
    m->run_until_ns(m->time_ns() + 20000000);
    CHECK(read_word(*m, rcc_csr_reg) == 0);
    poke(*m, wwdg, 0xBFu); // T6 written as 0
    m->run_until_ns(m->time_ns() + 1000);
    CHECK(read_word(*m, rcc_csr_reg) == 0x44000000u);
}

TEST_CASE("RTC and BKP: LSE seconds, alarm on EXTI 17, write protection and the backup domain")
{
    auto m = idle_machine();
    REQUIRE(m);
    // Without PWR_CR.DBP the backup domain ignores writes.
    poke(*m, bkp + 0x04, 0xBEEF);
    CHECK(read_word(*m, bkp + 0x04) == 0);
    CHECK(has_diagnostic(m->take_diagnostics(), "PWR_CR.DBP"));
    poke(*m, pwr_cr, 1u << 8);
    poke(*m, bkp + 0x04, 0xBEEF);
    poke(*m, bkp + 0x28, 0x1234);
    CHECK(read_word(*m, bkp + 0x04) == 0xBEEFu);
    CHECK(read_word(*m, bkp + 0x28) == 0x1234u); // DR10

    poke(*m, rcc_bdcr, 1);                       // LSEON
    CHECK((read_word(*m, rcc_bdcr) & 2u) == 2u); // LSERDY
    poke(*m, rcc_bdcr, 1u | (1u << 8) | (1u << 15)); // RTCSEL = LSE, RTCEN
    poke(*m, rtc + 0x1C, 5);                     // outside configuration mode: ignored
    CHECK(has_diagnostic(m->take_diagnostics(), "configuration mode"));
    poke(*m, rtc + 0x04, 0x10);                  // CNF
    poke(*m, rtc + 0x0C, 0x7FFF);                // PRL: 32768 cycles per second
    poke(*m, rtc + 0x1C, 10);                    // CNT
    poke(*m, rtc + 0x20, 0);                     // ALRH (ALR resets to 0xFFFFFFFF)
    poke(*m, rtc + 0x24, 12);                    // ALRL
    poke(*m, rtc + 0x04, 0);                     // leave configuration mode
    CHECK((read_word(*m, rtc + 0x04) & 0x28u) == 0x28u); // RSF, RTOFF
    poke(*m, exti_imr, 1u << 17);
    poke(*m, exti_rtsr, 1u << 17);
    poke(*m, rtc, 1); // SECIE
    const std::uint64_t start = m->time_ns();
    m->run_until_ns(start + 500000000);
    CHECK(read_word(*m, rtc + 0x1C) == 10u);
    const std::uint32_t div = read_word(*m, rtc + 0x14);
    CHECK(div > 0x3F00u && div < 0x4100u); // half a second left
    m->run_until_ns(start + 1000100000);
    CHECK(read_word(*m, rtc + 0x1C) == 11u);
    CHECK((read_word(*m, rtc + 0x04) & 1u) == 1u);    // SECF
    CHECK((read_word(*m, nvic_ispr0) & 8u) == 8u);    // RTC interrupt (3) pending
    CHECK((read_word(*m, exti_pr) & (1u << 17)) == 0);
    m->run_until_ns(start + 2000100000);
    CHECK(read_word(*m, rtc + 0x1C) == 12u);
    CHECK((read_word(*m, rtc + 0x04) & 2u) == 2u);    // ALRF
    CHECK((read_word(*m, exti_pr) & (1u << 17)) != 0); // the alarm on EXTI 17
    CHECK((read_word(*m, nvic_ispr1) & (1u << 9)) != 0); // RTCAlarm (41) pending
    poke(*m, rtc + 0x04, 0);                           // clear the flags (rc_w0)
    CHECK((read_word(*m, rtc + 0x04) & 7u) == 0);

    // A system reset keeps the backup domain: the RTC counts on, BKP keeps its data.
    m->reset(ResetKind::Pin);
    CHECK(read_word(*m, bkp + 0x04) == 0xBEEFu);
    CHECK(read_word(*m, rtc) == 0); // CRH is reset
    m->run_until_ns(start + 3000100000);
    CHECK(read_word(*m, rtc + 0x1C) == 13u);
    CHECK((read_word(*m, rcc_bdcr) & 0x8101u) == 0x8101u);

    // BDRST clears the backup domain.
    poke(*m, pwr_cr, 1u << 8);
    poke(*m, rcc_bdcr, 1u << 16);
    poke(*m, rcc_bdcr, 0);
    CHECK(read_word(*m, bkp + 0x04) == 0);
    CHECK(read_word(*m, rcc_bdcr) == 0);
    CHECK(read_word(*m, rtc + 0x1C) == 0);

    // And a power-on reset too.
    poke(*m, pwr_cr, 1u << 8);
    poke(*m, bkp + 0x08, 0x55);
    m->reset(ResetKind::PowerOn);
    CHECK(read_word(*m, bkp + 0x08) == 0);
}

TEST_CASE("RTC: LSI and HSE / 128 clocks and a prescaler change")
{
    auto m = idle_machine();
    REQUIRE(m);
    poke(*m, pwr_cr, 1u << 8);
    poke(*m, rcc_csr_reg, 1); // LSION
    poke(*m, rcc_bdcr, (2u << 8) | (1u << 15)); // LSI, RTCEN
    poke(*m, rtc + 0x04, 0x10);
    poke(*m, rtc + 0x0C, 39999); // one second at 40 kHz
    poke(*m, rtc + 0x04, 0);
    const std::uint64_t start = m->time_ns();
    m->run_until_ns(start + 3000100000);
    CHECK(read_word(*m, rtc + 0x1C) == 3u);
    // HSE / 128 = 62500 Hz with the Blue Pill's 8 MHz crystal; PRL 6249: ten counts a second.
    poke(*m, 0x40021000, 0x83u | (1u << 16)); // HSEON
    poke(*m, rcc_bdcr, (1u << 16));           // BDRST to change RTCSEL
    poke(*m, rcc_bdcr, (3u << 8) | (1u << 15));
    poke(*m, rtc + 0x04, 0x10);
    poke(*m, rtc + 0x0C, 6249);
    poke(*m, rtc + 0x04, 0);
    const std::uint64_t again = m->time_ns();
    m->run_until_ns(again + 1000100000);
    CHECK(read_word(*m, rtc + 0x1C) == 10u);
    // Without RTCEN the counter stands still.
    poke(*m, rcc_bdcr, 3u << 8);
    m->run_until_ns(again + 2000100000);
    CHECK(read_word(*m, rtc + 0x1C) == 10u);
}

TEST_CASE("an STM32CubeIDE HAL project keeps RTC time and start count through an IWDG reset")
{
    auto m = machine_with("hal_rtc_watchdog");
    REQUIRE(m);
    m->set_pin_input(0, 1, true); // the button on PA1 is released (pull-up)
    std::string text;
    auto run_to = [&](std::uint64_t ns) {
        m->run_until_ns(ns);
        for (const auto& b : m->take_uart_bytes()) {
            text += static_cast<char>(b.value);
        }
    };
    run_to(3500000000ull);
    CHECK(text.find("power-on, start 1") != std::string::npos);
    CHECK(text.find("12:00:03") != std::string::npos);
    CHECK(text.find("watchdog") == std::string::npos); // refreshed every 100 ms
    // Holding the button stops the refresh: the IWDG resets the chip within a second.
    m->set_pin_input(0, 1, false);
    run_to(4700000000ull);
    m->set_pin_input(0, 1, true);
    run_to(6500000000ull);
    CHECK(text.find("reset by the watchdog, start 2") != std::string::npos);
    // The RTC counted on through the reset and was not set again.
    const auto after = text.find("start 2");
    REQUIRE(after != std::string::npos);
    CHECK(text.find("12:00:06", after) != std::string::npos);
    CHECK(text.find("12:00:00", after) == std::string::npos);
    std::size_t resets = 0;
    for (const auto& d : m->take_diagnostics()) {
        resets += d.text.find("independent watchdog") != std::string::npos ? 1 : 0;
        CHECK(d.text.find("not simulated") == std::string::npos);
    }
    CHECK(resets == 1);
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
