// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// STM32F407 machine of experiment E-12 (SN-294): the owned test firmware of
// examples/firmware/f4_* in the machine without a circuit, and the registers
// of the clock tree, GPIO, SYSCFG, EXTI and the serial ports.
//
//   f4_tests <repository-dir> [filter]
#include "test_support.hpp"

#include "emu/elf.hpp"
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
    set(m, 0x40000000u + 0x2C, 999); // TIM2_ARR
    CHECK(reg(m, 0x40000000u + 0x2C) == 999);
    const auto told = m.take_diagnostics();
    REQUIRE(told.size() == 1);
    CHECK(told[0].text.find("TIM2 is not simulated yet") != std::string::npos);
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

int main(int argc, char** argv)
{
    if (argc < 2) {
        std::fprintf(stderr, "usage: f4_tests <repository-dir> [filter]\n");
        return 2;
    }
    repository = argv[1];
    return simnodus::test::run_all(argc > 2 ? argv[2] : "");
}
