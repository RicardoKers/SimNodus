// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// STM32F407 machine of experiment E-12 (SN-294): the owned test firmware of
// examples/firmware/f4_* in the machine without a circuit, and the registers
// of the clock tree, GPIO, SYSCFG, EXTI, the serial ports, the timers, the
// ADCs and the DAC (SN-297).
//
//   f4_tests <repository-dir> [filter]
#include "test_support.hpp"

#include "emu/elf.hpp"
#include "emu/gdb_server.hpp"
#include "emu/stm32f407.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <vector>

using namespace simnodus::emu;

namespace {

std::filesystem::path repository;

constexpr int A = 0, B = 1, C = 2, D = 3;
constexpr std::uint64_t ms = 1000000;

std::unique_ptr<Stm32F407> machine_with(const char* firmware, F4MachineOptions options = {})
{
    std::string error;
    const auto image = read_elf((repository / "examples" / "firmware" / firmware / (std::string(firmware) + ".elf")).string(), error);
    if (!image) {
        std::printf("  %s\n", error.c_str());
    }
    REQUIRE(image.has_value());
    auto machine = std::make_unique<Stm32F407>(options);
    REQUIRE(machine->load(*image, error));
    return machine;
}

// What a run gave: the level changes of each pin, the text of each serial port, the diagnostics.
struct Trace {
    std::map<std::pair<int, int>, std::vector<std::pair<std::uint64_t, bool>>> pins;
    std::map<int, std::string> text;
    std::vector<std::string> diagnostics;

    void take(Stm32F407& m)
    {
        for (const auto& e : m.take_pin_events()) {
            auto& changes = pins[{e.port, e.pin}];
            if (changes.empty() || changes.back().second != e.config.level) {
                changes.emplace_back(e.time_ns, e.config.level);
            }
        }
        for (const auto& b : m.take_uart_bytes()) {
            text[b.usart] += static_cast<char>(b.value);
        }
        for (const auto& d : m.take_diagnostics()) {
            diagnostics.push_back(d.text);
        }
    }
    // The changes of a pin; none if it never changed.
    const std::vector<std::pair<std::uint64_t, bool>>& of(int port, int pin) const
    {
        static const std::vector<std::pair<std::uint64_t, bool>> nothing;
        const auto it = pins.find({port, pin});
        return it == pins.end() ? nothing : it->second;
    }
    bool told(const char* part) const
    {
        return std::any_of(diagnostics.begin(), diagnostics.end(),
            [&](const std::string& d) { return d.find(part) != std::string::npos; });
    }
};

Trace run(Stm32F407& m, std::uint64_t until_ns)
{
    Trace trace;
    m.run_until_ns(until_ns);
    trace.take(m);
    return trace;
}

std::uint32_t reg(Stm32F407& m, std::uint32_t address)
{
    std::uint32_t value = 0;
    REQUIRE(m.bus().peek(address, 4, value));
    return value;
}

void set(Stm32F407& m, std::uint32_t address, std::uint32_t value)
{
    REQUIRE(m.bus().write(address, 4, value));
}

// The shortest time between two changes of a pin: one bit of a serial frame.
// `skip` leaves out the first changes (the line rising when the port is turned on).
std::uint64_t shortest(const std::vector<std::pair<std::uint64_t, bool>>& changes, std::size_t skip = 0)
{
    std::uint64_t best = ~std::uint64_t{0};
    for (std::size_t i = skip + 1; i < changes.size(); ++i) {
        best = std::min(best, changes[i].first - changes[i - 1].first);
    }
    return best;
}

constexpr std::uint32_t rcc = 0x40023800, gpioa = 0x40020000, gpiob = 0x40020400, gpioc = 0x40020800,
                        gpiod = 0x40020C00, syscfg = 0x40013800, exti = 0x40013C00, usart1 = 0x40011000,
                        usart2 = 0x40004400;

} // namespace

TEST_CASE("f4: the blink firmware toggles its pins with the SysTick's exact periods at 16 MHz")
{
    auto m = machine_with("f4_blink");
    CHECK(m->hclk_hz() == 16000000u);
    CHECK(m->cpu().architecture() == Cpu::Architecture::CortexM4F);
    const Trace t = run(*m, 2600 * ms);
    CHECK(t.diagnostics.empty());
    const auto& led = t.of(C, 13);
    REQUIRE(led.size() >= 5);
    for (std::size_t i = 2; i < led.size(); ++i) { // the first entry is the pin becoming an output
        CHECK(led[i].first - led[i - 1].first == 500 * ms);
        CHECK(led[i].second != led[i - 1].second);
    }
    CHECK(led[1].first > 500 * ms);
    CHECK(led[1].first < 500 * ms + 20000); // the handler's latency: microseconds
    // PA0 is the counter's lowest bit: a change every 250 ms.
    const auto& bit0 = t.of(A, 0);
    REQUIRE(bit0.size() >= 8);
    for (std::size_t i = 2; i < bit0.size(); ++i) {
        // To the microsecond: the handler takes a few cycles more when it also toggles PC13.
        const std::uint64_t step = bit0[i].first - bit0[i - 1].first;
        CHECK(step > 250 * ms - 1000 && step < 250 * ms + 1000);
    }
    CHECK(t.of(A, 2).size() == 3); // output, then set at 1 s and cleared at 2 s
    // PB0: the busy loop of 1 000 000 turns, the same number of cycles every time
    // but for the interrupts that fall into it.
    const auto& loop = t.of(B, 0);
    REQUIRE(loop.size() >= 4);
    const std::uint64_t period = loop[2].first - loop[1].first;
    std::printf("  busy loop of 1 000 000 turns: %.3f ms (%.2f cycles a turn)\n", static_cast<double>(period) / 1e6,
        static_cast<double>(period) * 16e6 / 1e9 / 1e6);
    CHECK(period > 300 * ms);
    CHECK(period < 800 * ms);
    for (std::size_t i = 3; i < loop.size(); ++i) {
        const std::uint64_t other = loop[i].first - loop[i - 1].first;
        CHECK(other > period - ms / 100 && other < period + ms / 100);
    }
    // The pins as the registers set them.
    const F4PinConfig pc13 = m->pin_config(C, 13);
    CHECK(pc13.mode == 1);
    CHECK(!pc13.open_drain);
    CHECK(m->pin_config(A, 13).mode == 2); // the debug pins keep their reset state
    CHECK(m->pin_config(A, 15).pull == 1);
    CHECK(m->pin_config(D, 0).mode == 0);
}

TEST_CASE("f4: the clock tree follows the crystal on the diagram")
{
    // No crystal: the HSE never becomes ready, the firmware waits, and the log says why.
    {
        auto m = machine_with("f4_clock");
        const Trace t = run(*m, 200 * ms);
        CHECK(t.told("no crystal is connected between PH0"));
        CHECK(t.text.empty());
        CHECK(m->hclk_hz() == 16000000u);
        CHECK((reg(*m, rcc) & (1u << 17)) == 0); // HSERDY
        CHECK((reg(*m, rcc) & (1u << 16)) != 0); // HSEON
    }
    // The 25 MHz crystal the firmware was written for: 168 MHz, 115200 baud.
    {
        F4MachineOptions options;
        options.hse_hz = 25000000;
        auto m = machine_with("f4_clock", options);
        const Trace t = run(*m, 2100 * ms);
        CHECK(t.diagnostics.empty());
        CHECK(m->hclk_hz() == 168000000u);
        CHECK(m->sysclk_hz() == 168000000u);
        CHECK(t.text.at(1) == "SYSCLK 168000000 HCLK 168000000 PCLK1 42000000 PCLK2 84000000\r\ntick 1\r\ntick 2\r\n");
        CHECK((reg(*m, rcc + 0x08) & 0xCu) == 0x8u); // SWS: the PLL
        // One bit on PA9: BRR = 729 periods of PCLK2 (84 MHz), 115226 baud.
        const std::uint64_t bit = shortest(t.of(A, 9), 2);
        CHECK(bit >= 8678 && bit <= 8680);
        // PC13 toggles every 500 ms of the faster clock too.
        const auto& led = t.of(C, 13);
        REQUIRE(led.size() >= 4);
        CHECK(led[3].first - led[2].first == 500 * ms);
    }
    // An 8 MHz crystal with the same firmware: the PLL gives 53.76 MHz and the
    // baud rate is wrong (36872 baud), as on the bench.
    {
        F4MachineOptions options;
        options.hse_hz = 8000000;
        auto m = machine_with("f4_clock", options);
        const Trace t = run(*m, 400 * ms);
        CHECK(m->hclk_hz() == 53760000u);
        const std::uint64_t bit = shortest(t.of(A, 9), 2);
        CHECK(bit >= 27119 && bit <= 27122);
        CHECK(!t.told("wait state")); // 5 wait states are more than this clock needs
    }
    // The registers by hand: the HSI through the PLL, the prescalers, and the
    // warning about the flash latency.
    {
        Stm32F407 m;
        CHECK(reg(m, rcc + 0x04) == 0x24003010u);
        CHECK((reg(m, rcc + 0x74) & 0xFE000000u) == 0x0E000000u); // power-on: PORRSTF, PINRSTF, BORRSTF
        set(m, rcc + 0x04, 16u | (192u << 6) | (1u << 16)); // 16 MHz / 16 * 192 / 4 = 48 MHz
        set(m, rcc + 0x00, reg(m, rcc) | (1u << 24));
        CHECK((reg(m, rcc) & (1u << 25)) != 0); // PLLRDY
        set(m, rcc + 0x08, 2u | (0x9u << 4) | (4u << 10)); // the PLL, HCLK = SYSCLK / 4, PCLK1 = HCLK / 2
        CHECK(m.sysclk_hz() == 48000000u);
        CHECK(m.hclk_hz() == 12000000u);
        CHECK(m.take_diagnostics().empty());
        set(m, rcc + 0x08, 2u); // HCLK = 48 MHz with no wait state
        const auto told = m.take_diagnostics();
        REQUIRE(told.size() == 1);
        CHECK(told[0].text.find("FLASH_ACR has 0 wait state(s)") != std::string::npos);
        set(m, rcc + 0x74, 1u << 24); // RMVF
        CHECK((reg(m, rcc + 0x74) & 0xFE000000u) == 0);
    }
}

TEST_CASE("f4: floating point and DSP firmware prints its checksums, the same on every run")
{
    // G12b.5 compares these lines with the owner's chip; until then they
    // guard against changes. The same lines with one cycle per instruction:
    // nothing in them depends on when the interrupt falls.
    const std::string expected = "A 1A3EE223\r\nB 183ECD01\r\nC 596EC23A\r\nI 46B57D98\r\nx 40182DAE\r\ndone\r\n";
    for (const bool timing : {true, false}) {
        F4MachineOptions options;
        options.cortex_timing = timing;
        auto m = machine_with("f4_float", options);
        const Trace t = run(*m, 400 * ms);
        if (t.text.count(1) == 0 || t.text.at(1) != expected) {
            std::printf("  printed: %s\n", t.text.count(1) ? t.text.at(1).c_str() : "(nothing)");
        }
        CHECK(t.diagnostics.empty());
        REQUIRE(t.text.count(1) == 1);
        CHECK(t.text.at(1) == expected);
        CHECK((reg(*m, 0xE000ED88) & 0x00F00000u) == 0x00F00000u); // CPACR: the start-up turned the FPU on
    }
}

TEST_CASE("f4: a push button on PA0 runs the EXTI0 handler on both edges")
{
    auto m = machine_with("f4_button");
    Trace t = run(*m, 10 * ms);
    CHECK(m->pin_level(A, 0)); // the pull-up
    CHECK(!m->pin_level(C, 13));
    for (int press = 1; press <= 3; ++press) {
        m->set_pin_input(A, 0, false);
        m->run_until_ns(m->time_ns() + 5 * ms);
        CHECK(m->pin_level(C, 13));
        CHECK(m->pin_level(B, 0) == ((press & 1) != 0));
        CHECK(m->pin_level(B, 1) == ((press & 2) != 0));
        m->set_pin_input(A, 0, true);
        m->run_until_ns(m->time_ns() + 5 * ms);
        CHECK(!m->pin_level(C, 13));
    }
    t.take(*m);
    CHECK(t.diagnostics.empty());
    CHECK(reg(*m, exti + 0x14) == 0); // the handler cleared the pending bit
}

TEST_CASE("f4: GPIO registers, alternate functions and the external interrupt of any port")
{
    Stm32F407 m;
    // Reset values (RM0090 8.4).
    CHECK(reg(m, gpioa + 0x00) == 0xA8000000u);
    CHECK(reg(m, gpiob + 0x00) == 0x00000280u);
    CHECK(reg(m, gpioa + 0x0C) == 0x64000000u);
    CHECK(reg(m, gpioc + 0x00) == 0);
    // Output types: push-pull, and open drain released when high.
    set(m, gpiod + 0x00, 0x5u); // PD0, PD1 outputs
    set(m, gpiod + 0x04, 0x2u); // PD1 open drain
    set(m, gpiod + 0x14, 0x3u);
    CHECK(m.pin_level(D, 0));
    CHECK(!m.pin_level(D, 1)); // released, nothing pulls it up
    set(m, gpiod + 0x0C, 1u << 2); // PD1's pull-up
    CHECK(m.pin_level(D, 1));
    m.set_pin_input(D, 1, false); // the circuit holds it low
    CHECK(!m.pin_level(D, 1));
    CHECK((reg(m, gpiod + 0x10) & 3u) == 1u); // IDR
    // BSRR: set wins over reset; the register reads as zero.
    set(m, gpiod + 0x18, (1u << 16) | 1u);
    CHECK(m.pin_level(D, 0));
    set(m, gpiod + 0x18, 1u << 16);
    CHECK(!m.pin_level(D, 0));
    CHECK(reg(m, gpiod + 0x18) == 0);
    // Pull-down and pull-up of inputs; an analog pin reads 0.
    set(m, gpioc + 0x0C, (2u << 0) | (1u << 2)); // PC0 down, PC1 up
    CHECK(!m.pin_level(C, 0));
    CHECK(m.pin_level(C, 1));
    set(m, gpioc + 0x00, 3u << 2); // PC1 analog
    CHECK(!m.pin_level(C, 1));
    CHECK(m.pin_config(C, 1).mode == 3);
    m.take_pin_events();

    // A serial port reaches a pin only through its alternate function number.
    set(m, usart1 + 0x08, 139);                   // BRR
    set(m, usart1 + 0x0C, (1u << 13) | (1u << 3)); // UE, TE
    set(m, gpioa + 0x00, reg(m, gpioa) | (2u << 18)); // PA9 alternate function, number 0 (not the USART's)
    set(m, usart1 + 0x04, 'a');
    Trace t = run(m, 2 * ms);
    CHECK(t.text[1] == "a"); // sent, but not on the pin
    CHECK(!m.pin_level(A, 9));
    set(m, gpioa + 0x24, 7u << 4); // AFRH: PA9 is function 7
    CHECK(m.pin_level(A, 9));       // the idle line is high
    m.take_pin_events();
    set(m, usart1 + 0x04, 'b');
    t = run(m, 4 * ms);
    CHECK(t.text[1] == "b");
    CHECK(t.of(A, 9).size() >= 4); // the frame's edges
    // The same port on its other pin (PB6), and a second port on PD5.
    set(m, gpioa + 0x24, 0);
    set(m, gpiob + 0x00, (reg(m, gpiob) & ~(3u << 12)) | (2u << 12));
    set(m, gpiob + 0x20, 7u << 24);
    CHECK(m.pin_level(B, 6));
    set(m, usart1 + 0x04, 'c');
    t = run(m, 6 * ms);
    CHECK(t.of(B, 6).size() >= 4);
    CHECK(t.of(A, 9).size() <= 1);
    set(m, gpiod + 0x00, 2u << 10);
    set(m, gpiod + 0x20, 7u << 20);
    set(m, usart2 + 0x08, 139);
    set(m, usart2 + 0x0C, (1u << 13) | (1u << 3));
    set(m, usart2 + 0x04, 'd');
    t = run(m, 8 * ms);
    CHECK(t.text[2] == "d");
    CHECK(t.of(D, 5).size() >= 4);
    // Oversampling by 8 (CR1.OVER8): the same BRR is twice the baud rate... with the fraction's three bits.
    m.take_pin_events();
    set(m, usart2 + 0x0C, (1u << 15) | (1u << 13) | (1u << 3));
    set(m, usart2 + 0x08, (8u << 4) | 5u); // 8 + 5/8: 69 periods of PCLK a bit
    set(m, usart2 + 0x04, 'U');             // 0x55: every bit an edge
    t = run(m, 10 * ms);
    const std::uint64_t bit = shortest(t.of(D, 5));
    CHECK(bit >= 4312 && bit <= 4313); // 69 / 16 MHz

    // Receiving: a byte from a terminal sets RXNE; frames from the pin too.
    set(m, usart1 + 0x0C, (1u << 13) | (1u << 3) | (1u << 2)); // and RE
    m.uart_receive(1, 'x');
    m.run_until_ns(12 * ms);
    CHECK((reg(m, usart1) & (1u << 5)) != 0);
    CHECK((reg(m, usart1 + 0x04) & 0xFFu) == 'x');

    // EXTI line 3 on port D through SYSCFG_EXTICR1, falling edge, interrupt 9 pending in the NVIC.
    set(m, syscfg + 0x08, 3u << 12);
    set(m, exti + 0x0C, 1u << 3); // FTSR
    set(m, exti + 0x00, 1u << 3); // IMR
    m.set_pin_input(D, 3, true);
    CHECK(reg(m, exti + 0x14) == 0);
    m.set_pin_input(A, 3, false); // another port's pin 3: not this line
    m.set_pin_input(A, 3, true);
    m.set_pin_input(A, 3, false);
    CHECK(reg(m, exti + 0x14) == 0);
    m.set_pin_input(D, 3, false);
    CHECK(reg(m, exti + 0x14) == (1u << 3));
    CHECK((reg(m, 0xE000E200) & (1u << 9)) != 0); // NVIC ISPR0: EXTI3
    set(m, exti + 0x14, 1u << 3);
    CHECK(reg(m, exti + 0x14) == 0);
    // A peripheral that is not simulated yet says so once, and keeps its registers.
    m.take_diagnostics();
    set(m, 0x40013000u + 0x0C, 0x5A); // SPI1_DR
    CHECK(reg(m, 0x40013000u + 0x0C) == 0x5A);
    const auto told = m.take_diagnostics();
    REQUIRE(told.size() == 1);
    CHECK(told[0].text.find("SPI1 is not simulated yet") != std::string::npos);
    // The chip's identity.
    CHECK(reg(m, 0xE0042000) == 0x10076413u);
    std::uint32_t kib = 0;
    REQUIRE(m.bus().peek(0x1FFF7A22, 2, kib));
    CHECK(kib == 512);
    CHECK(reg(m, 0xE000ED00) == 0x410FC241u);
}

TEST_CASE("f4: resets record their cause and two runs give the same trace")
{
    auto m = machine_with("f4_blink");
    m->run_until_ns(10 * ms);
    set(*m, 0xE000ED0C, 0x05FA0004u); // AIRCR: SYSRESETREQ
    m->run_until_ns(20 * ms);
    CHECK((reg(*m, rcc + 0x74) & (1u << 28)) != 0); // SFTRSTF
    m->reset(ResetKind::Pin);
    CHECK((reg(*m, rcc + 0x74) & (1u << 26)) != 0);
    // The same firmware twice: the same pin changes at the same times.
    std::string first;
    for (int attempt = 0; attempt < 2; ++attempt) {
        auto again = machine_with("f4_float");
        std::string trace;
        again->run_until_ns(200 * ms);
        for (const auto& e : again->take_pin_events()) {
            trace += std::to_string(e.time_ns) + ":" + std::to_string(e.port * 16 + e.pin) + (e.config.level ? "H " : "L ");
        }
        for (const auto& b : again->take_uart_bytes()) {
            trace += std::to_string(b.time_ns) + ":" + std::to_string(b.value) + " ";
        }
        CHECK(!trace.empty());
        if (attempt == 0) {
            first = trace;
        } else {
            CHECK(trace == first);
        }
    }
}

TEST_CASE("f4: NRST holds the chip in reset and BOOT0 high keeps it from starting")
{
    auto m = machine_with("f4_blink");
    m->run_until_ns(600 * ms);
    CHECK(m->pin_level(C, 13)); // the first toggle, at 500 ms
    m->set_reset_pin(true);
    CHECK(m->held());
    CHECK(m->pin_config(C, 13).mode == 0); // the pins are inputs again
    const std::uint64_t instructions = m->cpu().instructions();
    m->run_until_ns(1600 * ms); // time passes, nothing runs
    CHECK(m->time_ns() >= 1600 * ms);
    CHECK(m->cpu().instructions() == instructions);
    m->set_reset_pin(false);
    CHECK(!m->held());
    CHECK((reg(*m, rcc + 0x74) & (1u << 26)) != 0); // PINRSTF
    m->run_until_ns(1600 * ms + 600 * ms);
    CHECK(m->pin_level(C, 13)); // 500 ms after the release: the firmware started again
    m->take_diagnostics();
    // BOOT0 high when a reset ends: the system bootloader is not there, and the log says so once.
    m->set_boot0(true);
    m->reset(ResetKind::Pin);
    CHECK(m->held());
    Trace t = run(*m, m->time_ns() + 600 * ms);
    CHECK(t.told("BOOT0 is high at reset"));
    CHECK(!m->pin_level(C, 13));
    m->reset(ResetKind::Pin);
    CHECK(m->take_diagnostics().empty());
    m->set_boot0(false);
    m->reset(ResetKind::Pin);
    CHECK(!m->held());
}

TEST_CASE("f4: the debugger sees the core's registers and the FPU's")
{
    F4MachineOptions options;
    auto m = machine_with("f4_float", options);
    m->run_until_ns(50 * ms);
    const auto target = make_gdb_target(*m);
    REQUIRE(target != nullptr);
    const std::string xml = target->target_xml();
    CHECK(xml.find("org.gnu.gdb.arm.m-profile") != std::string::npos);
    CHECK(xml.find("org.simnodus.fpu") != std::string::npos);
    CHECK(xml.find("name=\"s31\"") != std::string::npos);
    CHECK(xml.find("name=\"fpscr\"") != std::string::npos);
    CHECK(xml.rfind("</target>") == xml.size() - 9);
    std::uint32_t value = 0;
    REQUIRE(target->read_register(15, value));
    CHECK(value >= 0x08000000u);
    CHECK(value < 0x08080000u);
    REQUIRE(target->read_register(13, value));
    CHECK(value > 0x20000000u);
    CHECK(value <= 0x20020000u);
    REQUIRE(target->write_register(21 + 7, 0x40490FDBu)); // S7
    CHECK(m->cpu().fp_reg(7) == 0x40490FDBu);
    REQUIRE(target->read_register(21 + 7, value));
    CHECK(value == 0x40490FDBu);
    REQUIRE(target->write_register(53, 0x00C00000u)); // FPSCR: round toward zero
    CHECK(m->cpu().fpscr() == 0x00C00000u);
    REQUIRE(target->read_register(53, value));
    CHECK(value == 0x00C00000u);
    CHECK(!target->read_register(54, value));
    // Memory: the flash is read, the SRAM written, the flash written as `load` does.
    std::uint8_t byte = 0;
    REQUIRE(target->read_byte(0x08000000, byte));
    bool loaded = false;
    REQUIRE(target->write_memory(0x20000100, std::string("\x12\x34", 2), loaded));
    CHECK(!loaded);
    REQUIRE(target->write_memory(0x08070000, std::string("\xAA", 1), loaded));
    CHECK(loaded);
    REQUIRE(target->read_byte(0x08070000, byte));
    CHECK(byte == 0xAA);
    // The STM32F103's description has no FPU.
    Stm32F103 f1;
    const auto f1_target = make_gdb_target(f1);
    CHECK(std::string(f1_target->target_xml()).find("org.simnodus.fpu") == std::string::npos);
    CHECK(!f1_target->read_register(21, value));
}

namespace {

constexpr std::uint32_t tim1 = 0x40010000, tim2 = 0x40000000, tim3 = 0x40000400, tim4 = 0x40000800,
                        tim6 = 0x40001000, tim9 = 0x40014000, adc1 = 0x40012000, adc_common = 0x40012300,
                        dac = 0x40007400;
constexpr int E = 4;

// A firmware that only waits (`b .`), so that the registers are the test's.
void idle(Stm32F407& m)
{
    ElfImage image;
    image.entry = 0x08000009;
    ElfSegment code;
    code.physical = code.virtual_address = 0x08000000;
    code.data = {0x00, 0x00, 0x02, 0x20, 0x09, 0x00, 0x00, 0x08, 0xFE, 0xE7};
    code.memory_size = static_cast<std::uint32_t>(code.data.size());
    image.segments.push_back(code);
    std::string error;
    REQUIRE(m.load(image, error));
}

// The durations of the high and of the low pulses of a pin, in order.
std::pair<std::vector<std::uint64_t>, std::vector<std::uint64_t>> pulses(
    const std::vector<std::pair<std::uint64_t, bool>>& changes)
{
    std::pair<std::vector<std::uint64_t>, std::vector<std::uint64_t>> out;
    for (std::size_t i = 1; i < changes.size(); ++i) {
        (changes[i - 1].second ? out.first : out.second).push_back(changes[i].first - changes[i - 1].first);
    }
    return out;
}

} // namespace

TEST_CASE("f4: TIM1 complementary PWM with dead time, a 32-bit TIM2 and the timers' shapes (SN-297)")
{
    Stm32F407 m; // HSI: 16 MHz, 62.5 ns a cycle
    idle(m);
    // PA8 = TIM1_CH1, PA7 = TIM1_CH1N (AF1), PA5 = TIM2_CH1 (AF1).
    set(m, gpioa + 0x00, 0xA8000000u | (2u << 16) | (2u << 14) | (2u << 10));
    set(m, gpioa + 0x20, (1u << 28) | (1u << 20));
    set(m, gpioa + 0x24, 1u);
    set(m, tim1 + 0x2C, 159);                 // 160 counts: 10 us
    set(m, tim1 + 0x34, 40);                  // CCR1
    set(m, tim1 + 0x18, 6u << 4);             // PWM 1
    set(m, tim1 + 0x44, (1u << 15) | 16u);    // MOE, a dead time of 16 clock periods: 1 us
    set(m, tim1 + 0x20, 0x5u);                // CC1E, CC1NE
    set(m, tim1 + 0x00, 1u);
    set(m, tim2 + 0x2C, 0x1FFFFu);            // 131072 counts: 8.192 ms, more than 16 bits
    set(m, tim2 + 0x34, 0x10000u);
    set(m, tim2 + 0x18, 6u << 4);
    set(m, tim2 + 0x20, 0x1u);
    set(m, tim2 + 0x00, 1u);
    CHECK(reg(m, tim2 + 0x2C) == 0x1FFFFu);
    set(m, tim3 + 0x2C, 0x1FFFFu);
    CHECK(reg(m, tim3 + 0x2C) == 0xFFFFu); // TIM3 has 16 bits
    Trace t;
    m.run_until_ns(30 * ms);
    t.take(m);
    const auto& main_out = t.of(A, 8);
    const auto& complement = t.of(A, 7);
    REQUIRE(main_out.size() > 1000);
    REQUIRE(complement.size() > 1000);
    const auto [high, low] = pulses(main_out);
    const auto [high_n, low_n] = pulses(complement);
    for (std::size_t i = 4; i < 1000; ++i) {
        CHECK(high[i] == 1500u);   // 40 counts less the dead time: 24 cycles
        CHECK(low[i] == 8500u);
        CHECK(high_n[i] == 6500u); // 120 counts less the dead time
        CHECK(low_n[i] == 3500u);
    }
    // Neither output turns on until 1 us after the other turned off.
    std::size_t checked = 0;
    for (std::size_t i = 10; i < 200; ++i) {
        if (!main_out[i].second) {
            continue;
        }
        const auto before = std::find_if(complement.rbegin(), complement.rend(),
            [&](const auto& c) { return c.first < main_out[i].first; });
        REQUIRE(before != complement.rend());
        CHECK(!before->second);
        CHECK(main_out[i].first - before->first == 1000u);
        ++checked;
    }
    CHECK(checked > 50);
    const auto [wide_high, wide_low] = pulses(t.of(A, 5));
    REQUIRE(wide_high.size() >= 3);
    CHECK(wide_high[1] == 4096000u);
    CHECK(wide_low[1] == 4096000u);
    CHECK(reg(m, tim2 + 0x24) <= 0x1FFFFu);
    CHECK(t.diagnostics.empty());

    // A complementary output alone follows the reference (it is not inverted).
    Stm32F407 alone;
    idle(alone);
    set(alone, gpioa + 0x00, 0xA8000000u | (2u << 14));
    set(alone, gpioa + 0x20, 1u << 28);
    set(alone, tim1 + 0x2C, 159);
    set(alone, tim1 + 0x34, 40);
    set(alone, tim1 + 0x18, 6u << 4);
    set(alone, tim1 + 0x44, 1u << 15);
    set(alone, tim1 + 0x20, 0x4u); // CC1NE only
    set(alone, tim1 + 0x00, 1u);
    Trace n;
    alone.run_until_ns(ms);
    n.take(alone);
    const auto [alone_high, alone_low] = pulses(n.of(A, 7));
    REQUIRE(alone_high.size() > 50);
    CHECK(alone_high[10] == 2500u);
    CHECK(alone_low[10] == 7500u);

    // What each timer has: the basic timers no channels, TIM9 two, TIM10 one.
    set(m, tim6 + 0x18, 0xFFFFu);
    CHECK(reg(m, tim6 + 0x18) == 0);
    set(m, tim9 + 0x18, 0xFFFFu);
    set(m, tim9 + 0x1C, 0xFFFFu);
    CHECK(reg(m, tim9 + 0x18) == 0xFFFFu);
    CHECK(reg(m, tim9 + 0x1C) == 0);
    set(m, 0x40014400u + 0x18, 0xFFFFu); // TIM10
    CHECK(reg(m, 0x40014400u + 0x18) == 0xFFu);
    // The timer clock is twice PCLK1 when APB1 is divided: HCLK / 2 here.
    Stm32F407 slow;
    idle(slow);
    set(slow, rcc + 0x08, 5u << 10); // PPRE1: divide by 4
    set(slow, tim3 + 0x2C, 0xFFFF);
    set(slow, tim3 + 0x00, 1u);
    slow.run_until_ns(ms);
    const std::uint32_t count = reg(slow, tim3 + 0x24);
    CHECK(count >= 7999);
    CHECK(count <= 8001);
    (void)E;
}

TEST_CASE("f4: input capture on both edges, the encoder interface and the shared interrupt lines (SN-297)")
{
    Stm32F407 m;
    idle(m);
    m.set_pin_input(A, 6, false);
    m.set_pin_input(B, 6, false);
    m.set_pin_input(B, 7, false);
    set(m, gpioa + 0x00, 0xA8000000u | (2u << 12)); // PA6 = TIM3_CH1 (AF2)
    set(m, gpioa + 0x20, 2u << 24);
    set(m, tim3 + 0x28, 15);                 // 1 us a count
    set(m, tim3 + 0x14, 1u);                 // UG: the prescaler takes its value now
    set(m, tim3 + 0x18, 1u);                 // CC1S: TI1
    set(m, tim3 + 0x20, 0xBu);               // CC1E with CC1P and CC1NP: both edges
    set(m, tim3 + 0x00, 1u);
    m.run_until_ns(100000);
    m.set_pin_input(A, 6, true);
    CHECK((reg(m, tim3 + 0x10) & 2u) != 0);
    std::uint32_t captured = reg(m, tim3 + 0x34);
    CHECK(captured >= 99);
    CHECK(captured <= 101);
    set(m, tim3 + 0x10, 0);
    m.run_until_ns(250000);
    m.set_pin_input(A, 6, false);
    CHECK((reg(m, tim3 + 0x10) & 2u) != 0);
    captured = reg(m, tim3 + 0x34);
    CHECK(captured >= 249);
    CHECK(captured <= 251);

    // An encoder on PB6 and PB7 (TIM4, AF2): four counts for a cycle of the two signals.
    set(m, gpiob + 0x00, 0x00000280u | (2u << 12) | (2u << 14));
    set(m, gpiob + 0x20, (2u << 24) | (2u << 28));
    set(m, tim4 + 0x18, 0x0101u);
    set(m, tim4 + 0x08, 3u); // encoder mode 3
    set(m, tim4 + 0x00, 1u);
    m.run_until_ns(300000);
    CHECK(reg(m, tim4 + 0x24) == 0); // the clock does not count
    const auto forward = [&] {
        m.set_pin_input(B, 6, true);
        m.set_pin_input(B, 7, true);
        m.set_pin_input(B, 6, false);
        m.set_pin_input(B, 7, false);
    };
    forward();
    CHECK(reg(m, tim4 + 0x24) == 4);
    CHECK((reg(m, tim4 + 0x00) & 0x10u) == 0);
    forward();
    CHECK(reg(m, tim4 + 0x24) == 8);
    m.set_pin_input(B, 7, true); // the other way
    m.set_pin_input(B, 6, true);
    m.set_pin_input(B, 7, false);
    m.set_pin_input(B, 6, false);
    CHECK(reg(m, tim4 + 0x24) == 4);
    CHECK((reg(m, tim4 + 0x00) & 0x10u) != 0); // DIR: down
    set(m, tim4 + 0x24, 0);
    m.set_pin_input(B, 7, true);
    CHECK(reg(m, tim4 + 0x24) == 0xFFFFu); // under zero: the auto-reload value, with an update event
    CHECK((reg(m, tim4 + 0x10) & 1u) != 0);
    // Mode 1 counts on the second input's edges only.
    m.set_pin_input(B, 7, false);
    set(m, tim4 + 0x24, 100);
    set(m, tim4 + 0x08, 1u);
    forward();
    CHECK(reg(m, tim4 + 0x24) == 102);
    CHECK(m.take_diagnostics().empty());

    // TIM9 shares interrupt 24 with TIM1's break, TIM6 has 54 with the DAC.
    set(m, tim9 + 0x2C, 99);
    set(m, tim9 + 0x0C, 1u);
    set(m, tim9 + 0x00, 1u);
    set(m, tim6 + 0x2C, 99);
    set(m, tim6 + 0x0C, 1u);
    set(m, tim6 + 0x00, 1u);
    CHECK((reg(m, 0xE000E200) & (1u << 24)) == 0);
    m.run_until_ns(400000);
    CHECK((reg(m, 0xE000E200) & (1u << 24)) != 0);
    CHECK((reg(m, 0xE000E204) & (1u << 22)) != 0);
    CHECK((reg(m, 0xE000E200) & (1u << 25)) == 0);
}

TEST_CASE("f4: the ADC's resolutions, scan with EOCS, overrun, injected channels and a timer trigger (SN-297)")
{
    Stm32F407 m;
    idle(m);
    m.set_analog_source([](int port, int pin, std::uint64_t) {
        return port == A && pin == 1 ? 1.0 : port == C && pin == 0 ? 2.5 : 0.0;
    });
    set(m, adc1 + 0x08, 1u);                  // ADON
    set(m, adc1 + 0x34, 1u);                  // SQ1: channel 1 (PA1)
    set(m, adc1 + 0x08, 1u | (1u << 30));     // SWSTART
    CHECK((reg(m, adc1 + 0x00) & 2u) == 0);
    m.run_until_ns(1600);                     // 15 ADC cycles at 8 MHz are 1875 ns (and `b .` takes 3 cycles)
    CHECK((reg(m, adc1 + 0x00) & 2u) == 0);
    m.run_until_ns(2000);
    CHECK((reg(m, adc1 + 0x00) & 0x12u) == 0x12u); // EOC, STRT
    CHECK(reg(m, adc1 + 0x4C) == 1241);       // floor(1.0 / 3.3 * 4096), without a side effect
    std::uint32_t value = 0;
    REQUIRE(m.bus().read(adc1 + 0x4C, 4, value));
    CHECK((reg(m, adc1 + 0x00) & 2u) == 0);   // reading DR clears EOC
    // 8 bits: 3 + 8 cycles, floor(1.0 / 3.3 * 256).
    set(m, adc1 + 0x04, 2u << 24);
    set(m, adc1 + 0x08, 1u | (1u << 30));
    m.run_until_ns(2000 + 1150);
    CHECK((reg(m, adc1 + 0x00) & 2u) == 0);
    m.run_until_ns(2000 + 1400);
    CHECK(reg(m, adc1 + 0x4C) == 77);
    REQUIRE(m.bus().read(adc1 + 0x4C, 4, value));
    // Left aligned, 12 bits.
    set(m, adc1 + 0x04, 0);
    set(m, adc1 + 0x08, 1u | (1u << 11) | (1u << 30));
    m.run_until_ns(10000);
    CHECK(reg(m, adc1 + 0x4C) == 1241u << 4);
    REQUIRE(m.bus().read(adc1 + 0x4C, 4, value));

    // Scan of two channels with EOC after each conversion.
    set(m, adc1 + 0x04, 1u << 8);             // SCAN
    set(m, adc1 + 0x2C, 1u << 20);            // two conversions
    set(m, adc1 + 0x34, 1u | (10u << 5));     // PA1, then PC0
    set(m, adc1 + 0x08, 1u | (1u << 10) | (1u << 30)); // EOCS
    m.run_until_ns(12000);
    CHECK(reg(m, adc1 + 0x4C) == 1241);
    REQUIRE(m.bus().read(adc1 + 0x4C, 4, value));
    m.run_until_ns(14000);
    CHECK(reg(m, adc1 + 0x4C) == 3103);       // floor(2.5 / 3.3 * 4096)
    REQUIRE(m.bus().read(adc1 + 0x4C, 4, value));
    CHECK((reg(m, adc1 + 0x00) & 0x20u) == 0);
    // Not reading the first result: the second is lost, with the overrun flag.
    set(m, adc1 + 0x08, 1u | (1u << 10) | (1u << 30));
    m.run_until_ns(20000);
    CHECK((reg(m, adc1 + 0x00) & 0x20u) != 0);
    CHECK(reg(m, adc1 + 0x4C) == 1241);
    CHECK((reg(m, adc_common + 0x00) & 0x20u) != 0); // CSR mirrors ADC1's flags
    REQUIRE(m.bus().read(adc1 + 0x4C, 4, value));
    set(m, adc1 + 0x00, 0);

    // Injected: the temperature sensor, with an offset.
    set(m, adc_common + 0x04, 1u << 23);      // TSVREFE
    set(m, adc1 + 0x38, 16u << 15);           // JSQ4: channel 16
    set(m, adc1 + 0x14, 43);                  // JOFR1
    set(m, adc1 + 0x08, 1u | (1u << 22));     // JSWSTART
    m.run_until_ns(24000);
    CHECK((reg(m, adc1 + 0x00) & 4u) != 0);   // JEOC
    CHECK(reg(m, adc1 + 0x3C) == 943 - 43);   // floor(0.76 / 3.3 * 4096) less the offset
    // VREFINT as a regular conversion, with the end-of-conversion interrupt.
    set(m, adc1 + 0x04, 1u << 5);             // EOCIE
    set(m, adc1 + 0x34, 17u);
    set(m, adc1 + 0x08, 1u | (1u << 30));
    m.run_until_ns(28000);
    CHECK(reg(m, adc1 + 0x4C) == 1501);       // floor(1.21 / 3.3 * 4096)
    CHECK((reg(m, 0xE000E200) & (1u << 18)) != 0);
    REQUIRE(m.bus().read(adc1 + 0x4C, 4, value));

    // TIM3's update event (TRGO) starts the regular group every 100 us.
    set(m, adc1 + 0x04, 0);
    set(m, adc1 + 0x34, 1u);
    set(m, adc1 + 0x08, 1u | (8u << 24) | (1u << 28));
    set(m, tim3 + 0x04, 2u << 4);             // MMS: update
    set(m, tim3 + 0x2C, 1599);
    set(m, tim3 + 0x00, 1u);
    const std::uint64_t start = m.time_ns();
    m.run_until_ns(start + 99000);
    CHECK((reg(m, adc1 + 0x00) & 2u) == 0);
    m.run_until_ns(start + 100000 + 1600);
    CHECK((reg(m, adc1 + 0x00) & 2u) == 0);
    m.run_until_ns(start + 100000 + 2000);
    CHECK((reg(m, adc1 + 0x00) & 2u) != 0);
    CHECK(reg(m, adc1 + 0x4C) == 1241);
    // ADC3 has PC0 as its channel 10, and nothing on the channels of port F.
    const std::uint32_t adc3 = adc1 + 0x200;
    set(m, adc3 + 0x08, 1u);
    set(m, adc3 + 0x34, 10u);
    set(m, adc3 + 0x08, 1u | (1u << 30));
    m.run_until_ns(start + 110000);
    CHECK(reg(m, adc3 + 0x4C) == 3103);
    CHECK((reg(m, adc_common + 0x00) & (2u << 16)) != 0);
    // What is not simulated says so.
    set(m, adc1 + 0x08, 1u | (1u << 8));
    const auto told = m.take_diagnostics();
    REQUIRE(told.size() == 1);
    CHECK(told[0].text.find("ADC: DMA is not simulated yet") != std::string::npos);
}

TEST_CASE("f4: the DAC drives its pins with its codes, from the data, a timer or the triangle (SN-297)")
{
    Stm32F407 m;
    idle(m);
    set(m, dac + 0x08, 2048);                 // DHR12R1
    m.take_pin_events();
    set(m, dac + 0x00, 1u);                   // EN1
    auto events = m.take_pin_events();
    REQUIRE(events.size() == 1);
    CHECK(events[0].port == A);
    CHECK(events[0].pin == 4);
    CHECK(events[0].config.dac);
    CHECK(events[0].config.dac_buffered);
    CHECK(events[0].config.dac_code == 2048);
    CHECK(reg(m, dac + 0x2C) == 2048);
    set(m, dac + 0x00, 3u);                   // BOFF1: without the buffer
    CHECK(!m.pin_config(A, 4).dac_buffered);
    CHECK(!m.pin_config(A, 5).dac);
    // The alignments, and both channels at once.
    set(m, dac + 0x10, 0x80);                 // DHR8R1
    CHECK(m.pin_config(A, 4).dac_code == 0x800);
    set(m, dac + 0x0C, 0xFFF0);               // DHR12L1
    CHECK(m.pin_config(A, 4).dac_code == 0xFFF);
    set(m, dac + 0x00, 1u | (1u << 16));
    set(m, dac + 0x20, 100u | (200u << 16));  // DHR12RD
    CHECK(m.pin_config(A, 4).dac_code == 100);
    CHECK(m.pin_config(A, 5).dac_code == 200);
    CHECK(reg(m, dac + 0x14) == 200);
    // The ADC reads the DAC's pin: channel 4 is PA4.
    set(m, dac + 0x08, 2048);
    set(m, adc1 + 0x08, 1u);
    set(m, adc1 + 0x34, 4u);
    set(m, adc1 + 0x08, 1u | (1u << 30));
    m.run_until_ns(5000);
    CHECK(reg(m, adc1 + 0x4C) == 2048);

    // With a trigger the output waits for it: TIM6's update every 10 us.
    set(m, dac + 0x00, 1u | (1u << 2));       // TEN1, TSEL1 = TIM6
    set(m, dac + 0x08, 1000);
    CHECK(reg(m, dac + 0x2C) == 2048);
    set(m, tim6 + 0x04, 2u << 4);
    set(m, tim6 + 0x2C, 159);
    m.take_pin_events();
    const std::uint64_t start = m.time_ns();
    set(m, tim6 + 0x00, 1u);
    m.run_until_ns(start + 25000);
    CHECK(reg(m, dac + 0x2C) == 1000);
    events = m.take_pin_events();
    REQUIRE(events.size() == 1);
    CHECK(events[0].time_ns - start == 10000u);
    // The triangle: the data plus a counter that goes up to the amplitude and back.
    set(m, dac + 0x00, 0);
    set(m, dac + 0x08, 100);
    set(m, dac + 0x00, 1u | (1u << 2) | (2u << 6) | (1u << 8)); // amplitude 3
    m.take_pin_events();
    m.run_until_ns(m.time_ns() + 200000);
    std::vector<int> codes;
    for (const auto& e : m.take_pin_events()) {
        codes.push_back(e.config.dac_code);
    }
    REQUIRE(codes.size() >= 12);
    const std::vector<int> expected = {101, 102, 103, 102, 101, 100, 101, 102, 103, 102, 101, 100};
    CHECK(std::vector<int>(codes.begin(), codes.begin() + 12) == expected);
    // The software trigger.
    set(m, tim6 + 0x00, 0);
    set(m, dac + 0x00, 1u | (1u << 2) | (7u << 3));
    set(m, dac + 0x08, 77);
    CHECK(reg(m, dac + 0x2C) != 77);
    set(m, dac + 0x04, 1u);
    CHECK(reg(m, dac + 0x2C) == 77);
    // A reset releases the pin.
    m.reset(ResetKind::Pin);
    CHECK(!m.pin_config(A, 4).dac);
    CHECK(m.take_diagnostics().empty());
}

TEST_CASE("f4: the analog firmware: a timer starts the ADC, its code sets the PWM and the DAC (SN-297)")
{
    auto m = machine_with("f4_analog");
    double volts = 1.0;
    m->set_analog_source([&](int port, int pin, std::uint64_t) { return port == A && pin == 1 ? volts : 0.0; });
    Trace t = run(*m, 250 * ms);
    CHECK(t.diagnostics.empty());
    REQUIRE(t.text.count(1) == 1);
    CHECK(t.text.at(1).rfind("f4_analog\r\nadc 1241\r\nadc 1241\r\n", 0) == 0);
    // PA4 gives the code back; PA8 and PB13 have the duty cycle of 1241 / 4096
    // (242 of 800 counts) less 8 counts of dead time on each side.
    CHECK(m->pin_config(A, 4).dac);
    CHECK(m->pin_config(A, 4).dac_code == 1241);
    CHECK(m->pin_config(A, 4).dac_buffered);
    CHECK(m->pin_config(B, 13).mode == 2);
    CHECK(!m->pin_config(B, 13).af_input);
    const auto [high, low] = pulses(t.of(A, 8));
    const auto [high_n, low_n] = pulses(t.of(B, 13));
    REQUIRE(high.size() > 100);
    REQUIRE(high_n.size() > 100);
    CHECK(high.back() == 234u * 1000u / 16u);
    CHECK(high_n.back() == (800u - 242u - 8u) * 1000u / 16u);
    // PA5's triangle: one step every 100 us, from 1536 up to 2559 and back.
    int lowest = 4096, highest = -1;
    std::size_t steps = 0;
    for (const auto& e : m->take_pin_events()) {
        (void)e;
    }
    m->run_until_ns(m->time_ns() + 250 * ms);
    for (const auto& e : m->take_pin_events()) {
        if (e.port == A && e.pin == 5) {
            lowest = std::min(lowest, static_cast<int>(e.config.dac_code));
            highest = std::max(highest, static_cast<int>(e.config.dac_code));
            ++steps;
        }
    }
    CHECK(lowest == 1536);
    CHECK(highest == 1536 + 1023);
    CHECK(steps >= 2490);
    CHECK(steps <= 2510);
    // A new voltage: the next conversions follow it.
    volts = 2.5;
    t = run(*m, m->time_ns() + 150 * ms);
    CHECK(t.text.at(1).find("adc 3103\r\n") != std::string::npos);
    CHECK(m->pin_config(A, 4).dac_code == 3103);
}

int main(int argc, char** argv)
{
    if (argc < 2) {
        std::fprintf(stderr, "usage: f4_tests <repository-dir> [filter]\n");
        return 2;
    }
    repository = argv[1];
    return simnodus::test::run_all(argc > 2 ? argv[2] : "");
}
