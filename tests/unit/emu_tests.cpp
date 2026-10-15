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
    for (const auto& e : m.take_pin_events()) {
        if (e.port == 0 && e.pin == 1) {
            if (e.level) {
                rise = e.time_ns;
            } else if (rise) {
                pulses.push_back(e.time_ns - rise);
            }
        } else if (e.port == 2 && e.pin == 13) {
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
