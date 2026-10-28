// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// ESP32-C3 machine of the SimNodus emulator (experiment E-08, SN-244): the
// ROM rebuilt from Espressif's ROM ELF, unmodified firmware images booting
// through the ROM and the second-stage bootloader (ESP-IDF hello_world, the
// official MicroPython release), timing from SYSTIMER, the CPU reset, the
// MicroPython REPL with its files in the flash, and repeatable runs.
//
//   esp32c3_tests <repository-dir> [filter]
#include "test_support.hpp"

#include "emu/elf.hpp"
#include "emu/esp32c3.hpp"
#include "emu/esp32c3_rom.hpp"

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

using namespace simnodus::emu;
namespace fs = std::filesystem;

namespace {

fs::path repository;

std::vector<std::uint8_t> read_file(const fs::path& path)
{
    std::ifstream stream(path, std::ios::binary);
    return std::vector<std::uint8_t>((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
}

fs::path firmware(const char* name)
{
    return repository / "tests" / "experiments" / "esp32c3" / "firmware" / name;
}

// A machine with the ROM and `image` at flash offset 0.
std::unique_ptr<Esp32C3> boot(const std::vector<std::uint8_t>& image)
{
    std::string error;
    const auto rom = read_elf(repository / "third_party" / "esp-rom-elfs" / "esp32c3_rev3_rom.elf", error, ElfMachine::Riscv);
    auto machine = std::make_unique<Esp32C3>();
    if (!rom || !machine->load_rom(*rom, error) || !machine->load_flash(0, image, error)) {
        simnodus::test::fail(__FILE__, __LINE__, error);
        return nullptr;
    }
    machine->reset();
    return machine;
}

struct Line {
    std::uint64_t time_ns = 0; // when the newline was sent
    std::string text;
};

// Runs to `ns` and returns the lines of `port` (0 UART0, 2 USB).
std::vector<Line> run_lines(Esp32C3& machine, std::uint64_t ns, int port, std::string& partial)
{
    std::vector<Line> lines;
    machine.run_until_ns(ns);
    for (const auto& out : machine.take_output()) {
        if (out.port != port) {
            continue;
        }
        if (out.value == '\n') {
            lines.push_back({out.time_ns, partial});
            partial.clear();
        } else if (out.value != '\r') {
            partial.push_back(static_cast<char>(out.value));
        }
    }
    return lines;
}

bool has(const std::vector<Line>& lines, const std::string& text)
{
    for (const Line& line : lines) {
        if (line.text.find(text) != std::string::npos) {
            return true;
        }
    }
    return false;
}

std::uint64_t fnv1a(const std::vector<std::uint8_t>& bytes)
{
    std::uint64_t hash = 0xcbf29ce484222325ull;
    for (std::uint8_t b : bytes) {
        hash = (hash ^ b) * 0x100000001b3ull;
    }
    return hash;
}

} // namespace

TEST_CASE("esp32c3: the ROM rebuilt from the ELF equals the chip's ROM")
{
    std::string error;
    const auto elf = read_elf(repository / "third_party" / "esp-rom-elfs" / "esp32c3_rev3_rom.elf", error, ElfMachine::Riscv);
    REQUIRE(elf);
    std::vector<std::uint8_t> rom;
    REQUIRE(build_esp32c3_rom(*elf, rom, error));
    CHECK(rom.size() == esp32c3_rom_size);
    // FNV-1a of a ROM read from an ESP32-C3 (SHA-256 0de1e650...73f99,
    // docs/experiments/E-08-results.md).
    CHECK(fnv1a(rom) == 0x5acfba3daf0a7b11ull);
    // Not a ROM ELF: refused with a message.
    ElfImage other;
    CHECK(!build_esp32c3_rom(other, rom, error));
    CHECK(!error.empty());
}

TEST_CASE("esp32c3: ESP-IDF hello_world boots unmodified")
{
    auto machine = boot(read_file(firmware("hello_world.bin")));
    REQUIRE(machine);
    std::string partial;
    const auto lines = run_lines(*machine, 3'100'000'000ull, 0, partial);
    // The ROM, the second-stage bootloader and the application, as on the chip.
    CHECK(has(lines, "ESP-ROM:esp32c3-api1-20210207"));
    CHECK(has(lines, "rst:0x1 (POWERON),boot:0xc (SPI_FAST_FLASH_BOOT)"));
    CHECK(has(lines, "boot: ESP-IDF v5.5.2 2nd stage bootloader"));
    CHECK(has(lines, "boot: chip revision: v0.4"));
    CHECK(has(lines, "spi_flash: detected chip: gd"));
    CHECK(has(lines, "Hello world!"));
    CHECK(has(lines, "This is esp32c3 chip with 1 CPU core(s), WiFi/BLE, silicon revision v0.4, 2MB external flash"));
    CHECK(machine->cpu_hz() == 160'000'000);
    // vTaskDelay(1000 ms) between the countdown lines (FreeRTOS tick from SYSTIMER).
    std::vector<std::uint64_t> times;
    for (const Line& line : lines) {
        if (line.text.rfind("Restarting in ", 0) == 0) {
            times.push_back(line.time_ns);
        }
    }
    REQUIRE(times.size() >= 3);
    for (std::size_t i = 1; i < times.size(); ++i) {
        const double period_ms = static_cast<double>(times[i] - times[i - 1]) / 1e6;
        CHECK(period_ms > 999.0 && period_ms < 1001.0); // within 0.1 % (G8b.2)
    }
    CHECK(machine->take_diagnostics().empty());
}

TEST_CASE("esp32c3: esp_restart resets the CPU and the ROM says why")
{
    auto machine = boot(read_file(firmware("hello_world.bin")));
    REQUIRE(machine);
    std::string partial;
    const auto lines = run_lines(*machine, 11'300'000'000ull, 0, partial);
    CHECK(has(lines, "Restarting now."));
    CHECK(has(lines, "rst:0xc (RTC_SW_CPU_RST),boot:0xc (SPI_FAST_FLASH_BOOT)"));
    CHECK(machine->resets() == 1);
}

TEST_CASE("esp32c3: MicroPython REPL and files kept in the flash")
{
    auto machine = boot(read_file(firmware("ESP32_GENERIC_C3-20260824-v1.29.0.bin")));
    REQUIRE(machine);
    std::string partial;
    auto lines = run_lines(*machine, 400'000'000ull, 2, partial); // USB-Serial-JTAG
    CHECK(has(lines, "MicroPython v1.29.0 on 2026-08-24; ESP32C3 module with ESP32-C3"));
    CHECK(partial == ">>> ");
    machine->send(2, "print(6*7)\r");
    lines = run_lines(*machine, 500'000'000ull, 2, partial);
    CHECK(has(lines, "42"));
    machine->send(2, "f=open('main.py','w'); f.write('print(\"main.py ran\")'); f.close()\r");
    run_lines(*machine, 700'000'000ull, 2, partial);
    // A new machine started from this flash runs main.py at boot.
    const std::vector<std::uint8_t> flash = machine->flash();
    auto again = boot(flash);
    REQUIRE(again);
    std::string partial2;
    const auto lines2 = run_lines(*again, 400'000'000ull, 2, partial2);
    CHECK(has(lines2, "main.py ran"));
    CHECK(!has(lines2, "Performing initial setup")); // the file system was there
}

TEST_CASE("esp32c3: ESP-IDF blink toggles GPIO8 every 500 ms")
{
    auto machine = boot(read_file(firmware("blink.bin")));
    REQUIRE(machine);
    machine->run_until_ns(2'600'000'000ull);
    std::vector<std::uint64_t> edges;
    bool last = false;
    bool primed = false;
    for (const auto& e : machine->take_pin_events()) {
        if (e.pin != 8 || !e.state.output) {
            continue;
        }
        if (primed && e.state.level != last) {
            edges.push_back(e.time_ns);
        }
        last = e.state.level;
        primed = true;
    }
    REQUIRE(edges.size() >= 4);
    for (std::size_t i = 1; i < edges.size(); ++i) {
        const double period_ms = static_cast<double>(edges[i] - edges[i - 1]) / 1e6;
        CHECK(period_ms > 499.5 && period_ms < 500.5); // CONFIG_BLINK_PERIOD 500, within 0.1 % (G8b.2)
    }
    CHECK(machine->pin(8).output);
}

TEST_CASE("esp32c3: MicroPython drives pins, sleeps, counts edges and uses a UART")
{
    auto machine = boot(read_file(firmware("ESP32_GENERIC_C3-20260824-v1.29.0.bin")));
    REQUIRE(machine);
    std::string partial;
    run_lines(*machine, 300'000'000ull, 2, partial);
    machine->send(2, "from machine import Pin, UART; import time; p=Pin(5, Pin.OUT); c=0\r");
    run_lines(*machine, 350'000'000ull, 2, partial);
    machine->take_pin_events();
    machine->send(2, "[(p.value(i%2), time.sleep_ms(500)) for i in range(4)]\r");
    run_lines(*machine, 2'500'000'000ull, 2, partial);
    std::vector<std::uint64_t> edges;
    for (const auto& e : machine->take_pin_events()) {
        if (e.pin == 5 && e.state.output) {
            edges.push_back(e.time_ns);
        }
    }
    REQUIRE(edges.size() >= 3);
    for (std::size_t i = 1; i < edges.size(); ++i) {
        const double period_ms = static_cast<double>(edges[i] - edges[i - 1]) / 1e6;
        CHECK(period_ms > 499.5 && period_ms < 500.5); // time.sleep_ms(500) within 0.1 % (G8b.4)
    }
    // An input with its pull-up, a falling-edge interrupt, edges from the circuit.
    machine->send(2, "def h(q): global c; c+=1\r\r");
    run_lines(*machine, 2'600'000'000ull, 2, partial);
    machine->send(2, "b=Pin(4, Pin.IN, Pin.PULL_UP); b.irq(h, Pin.IRQ_FALLING); print(b.value())\r");
    auto lines = run_lines(*machine, 2'700'000'000ull, 2, partial);
    CHECK(has(lines, "1"));
    for (int i = 0; i < 3; ++i) {
        machine->set_pin_input(4, false);
        machine->run_until_ns(2'700'000'000ull + 20'000'000ull * static_cast<std::uint64_t>(2 * i + 1));
        machine->set_pin_input(4, true);
        machine->run_until_ns(2'700'000'000ull + 20'000'000ull * static_cast<std::uint64_t>(2 * i + 2));
    }
    machine->send(2, "print('edges', c)\r");
    lines = run_lines(*machine, 2'900'000'000ull, 2, partial);
    CHECK(has(lines, "edges 3"));
    // UART1: bytes from the circuit, bytes to it.
    machine->send(2, "u=UART(1, 115200, tx=6, rx=7)\r");
    run_lines(*machine, 3'000'000'000ull, 2, partial);
    machine->send(1, "abc");
    machine->send(2, "print(u.read()); u.write(b'xyz')\r");
    std::string uart1;
    machine->run_until_ns(3'100'000'000ull);
    std::string usb;
    for (const auto& out : machine->take_output()) {
        if (out.port == 1) {
            uart1.push_back(static_cast<char>(out.value));
        } else if (out.port == 2) {
            usb.push_back(static_cast<char>(out.value));
        }
    }
    CHECK(usb.find("b'abc'") != std::string::npos);
    CHECK(uart1 == "xyz");
}

TEST_CASE("esp32c3: a watchdog that runs out resets the chip with its reason")
{
    auto machine = boot(read_file(firmware("hello_world.bin")));
    REQUIRE(machine);
    machine->run_until_ns(100'000'000ull); // the application runs; it leaves the RTC watchdog off
    machine->take_output();
    // RTC watchdog: unlock, stage 0 = system reset after 6800 slow ticks (50 ms at 136 kHz).
    Bus& bus = machine->bus();
    bus.write(0x600080A8u, 4, 0x50D83AA1u); // RTC_CNTL_WDTWPROTECT
    bus.write(0x60008094u, 4, 6800);         // RTC_CNTL_WDTCONFIG1: stage 0 timeout
    bus.write(0x60008090u, 4, (1u << 31) | (3u << 28)); // WDTCONFIG0: enabled, stage 0 system reset
    std::string partial;
    const auto lines = run_lines(*machine, 200'000'000ull, 0, partial);
    CHECK(machine->reset_reason() == 0x9);
    CHECK(has(lines, "rst:0x9 (RTCWDT_SYS_RST),boot:0xc (SPI_FAST_FLASH_BOOT)"));
    // The reset button restarts as at power-on.
    auto fed = boot(read_file(firmware("hello_world.bin")));
    REQUIRE(fed);
    fed->run_until_ns(100'000'000ull);
    fed->press_reset();
    fed->run_until_ns(101'000'000ull);
    CHECK(fed->reset_reason() == 0x1);
    std::string partial2;
    const auto lines2 = run_lines(*fed, 200'000'000ull, 0, partial2);
    CHECK(has(lines2, "rst:0x1 (POWERON),boot:0xc (SPI_FAST_FLASH_BOOT)"));
}

TEST_CASE("esp32c3: ESP-IDF esp_timer, light sleep and FreeRTOS examples")
{
    auto timer = boot(read_file(firmware("esp_timer.bin")));
    REQUIRE(timer);
    std::string partial;
    auto lines = run_lines(*timer, 13'000'000'000ull, 0, partial);
    std::vector<std::uint64_t> periodic;
    for (const Line& line : lines) {
        if (line.text.find("example: Periodic timer called") != std::string::npos && periodic.size() < 9) {
            periodic.push_back(line.time_ns);
        }
    }
    REQUIRE(periodic.size() >= 9);
    for (std::size_t i = 1; i < 9; ++i) { // 500 ms until the one-shot timer at 5 s
        const double ms = static_cast<double>(periodic[i] - periodic[i - 1]) / 1e6;
        CHECK(ms > 499.5 && ms < 500.5);
    }
    CHECK(has(lines, "example: One-shot timer called"));
    // The example asserts that light sleep lasted 0.5 s by esp_timer.
    CHECK(has(lines, "example: Woke up from light sleep"));
    CHECK(has(lines, "example: Stopped and deleted timers"));
    CHECK(!has(lines, "Guru Meditation"));

    auto stats = boot(read_file(firmware("real_time_stats.bin")));
    REQUIRE(stats);
    std::string partial2;
    lines = run_lines(*stats, 1'300'000'000ull, 0, partial2);
    CHECK(has(lines, "| Task | Run Time | Percentage"));
    for (const char* task : {"| spin0 |", "| spin5 |", "| IDLE |"}) {
        CHECK(has(lines, task));
    }
}

TEST_CASE("esp32c3: Arduino-ESP32 sketch runs unchanged")
{
    auto machine = boot(read_file(firmware("arduino_serial_blink.bin")));
    REQUIRE(machine);
    std::string partial;
    auto lines = run_lines(*machine, 600'000'000ull, 0, partial);
    CHECK(has(lines, "Arduino on ESP32-C3"));
    CHECK(has(lines, "tick 2 at 535 ms"));
    machine->send(0, "abc");
    lines = run_lines(*machine, 1'100'000'000ull, 0, partial);
    CHECK(has(lines, "ABCtick")); // the echo in capitals, before the next line
    std::vector<std::uint64_t> edges;
    for (const auto& e : machine->take_pin_events()) {
        if (e.pin == 8 && e.state.output) {
            edges.push_back(e.time_ns);
        }
    }
    REQUIRE(edges.size() >= 4);
    const double ms = static_cast<double>(edges.back() - edges[edges.size() - 2]) / 1e6;
    CHECK(ms > 249.5 && ms < 250.5); // delay(250)
}

TEST_CASE("esp32c3: faults give the panic report and a diagnostic")
{
    const auto image = read_file(firmware("faults.bin"));
    struct Fault {
        const char* key;
        const char* report;
        const char* diagnostic;
    };
    const Fault faults[] = {
        {"i", "panic'ed (Illegal instruction)", "Exception 2 at PC"},
        {"l", "panic'ed (Load access fault)", "Exception 5 at PC"},
        {"a", "abort() was called at PC", "Exception 2 at PC"},
    };
    for (const Fault& fault : faults) {
        auto machine = boot(image);
        REQUIRE(machine);
        std::string partial;
        run_lines(*machine, 100'000'000ull, 0, partial);
        machine->send(0, fault.key);
        const auto lines = run_lines(*machine, 110'000'000ull, 0, partial);
        CHECK(has(lines, fault.report));
        CHECK(has(lines, "rst:0xc (RTC_SW_CPU_RST)")); // the panic handler restarts
        bool noted = false;
        for (const std::string& text : machine->take_diagnostics()) {
            noted = noted || text.find(fault.diagnostic) != std::string::npos;
        }
        CHECK(noted);
    }
}

TEST_CASE("esp32c3: MicroPython deep sleep wakes by the timer with a reset")
{
    auto machine = boot(read_file(firmware("ESP32_GENERIC_C3-20260824-v1.29.0.bin")));
    REQUIRE(machine);
    std::string partial;
    run_lines(*machine, 300'000'000ull, 2, partial);
    machine->send(2, "import machine; machine.deepsleep(1000)\r");
    auto lines = run_lines(*machine, 1'400'000'000ull, 2, partial); // asleep 1 s from about 300 ms
    CHECK(machine->reset_reason() == 0x5);
    CHECK(has(lines, "rst:0x5 (DSLEEP),boot:0xc (SPI_FAST_FLASH_BOOT)"));
    run_lines(*machine, 1'600'000'000ull, 2, partial);
    machine->send(2, "import machine; print('cause', machine.reset_cause(), machine.wake_reason())\r");
    lines = run_lines(*machine, 1'700'000'000ull, 2, partial);
    CHECK(has(lines, "cause 4 4")); // DEEPSLEEP_RESET, TIMER_WAKE
}

TEST_CASE("esp32c3: MicroPython PWM on LEDC and ADC readings")
{
    auto machine = boot(read_file(firmware("ESP32_GENERIC_C3-20260824-v1.29.0.bin")));
    REQUIRE(machine);
    std::string partial;
    run_lines(*machine, 300'000'000ull, 2, partial);
    // 1 kHz at 25 %, then 75 %, then 2 kHz: the pad's edges show period and duty.
    struct Step {
        const char* command;
        double period_ms;
        double high_ms;
    };
    const Step steps[] = {
        {"from machine import PWM, ADC, Pin; p=PWM(Pin(5), freq=1000, duty_u16=16384)\r", 1.0, 0.25},
        {"p.duty_u16(49152)\r", 1.0, 0.75},
        {"p.freq(2000)\r", 0.5, 0.375},
    };
    std::uint64_t t = 300'000'000ull;
    for (const Step& step : steps) {
        machine->send(2, step.command);
        run_lines(*machine, t + 50'000'000ull, 2, partial);
        machine->take_pin_events();
        run_lines(*machine, t + 60'000'000ull, 2, partial); // 10 ms of steady PWM
        std::vector<std::pair<std::uint64_t, bool>> edges;
        for (const auto& e : machine->take_pin_events()) {
            if (e.pin == 5 && e.state.output) {
                edges.emplace_back(e.time_ns, e.state.level);
            }
        }
        REQUIRE(edges.size() >= 6);
        std::size_t rise = 0;
        while (rise < edges.size() && !edges[rise].second) {
            ++rise;
        }
        REQUIRE(rise + 2 < edges.size());
        const double high = static_cast<double>(edges[rise + 1].first - edges[rise].first) / 1e6;
        const double period = static_cast<double>(edges[rise + 2].first - edges[rise].first) / 1e6;
        CHECK(std::abs(period - step.period_ms) < step.period_ms * 0.002);
        CHECK(std::abs(high - step.high_ms) < step.period_ms * 0.002);
        t += 100'000'000ull;
    }
    // ADC1: calibrated readings (read_uv) give the voltage on the pad.
    machine->set_pin_voltage(2, 1.25);
    machine->set_pin_voltage(3, 0.30);
    machine->send(2, "a=ADC(Pin(2), atten=ADC.ATTN_11DB); b=ADC(Pin(3), atten=ADC.ATTN_0DB)\r");
    run_lines(*machine, t + 50'000'000ull, 2, partial);
    machine->send(2, "print('uv', a.read_uv(), b.read_uv())\r");
    const auto lines = run_lines(*machine, t + 100'000'000ull, 2, partial);
    bool found = false;
    for (const Line& line : lines) {
        std::istringstream words(line.text);
        std::string tag;
        long a = 0, b = 0;
        if (words >> tag >> a >> b && tag == "uv") {
            found = true;
            CHECK(std::abs(a - 1'250'000L) < 12'500L); // within 1 %
            CHECK(std::abs(b - 300'000L) < 3'000L);
        }
    }
    CHECK(found);
}

TEST_CASE("esp32c3: two runs give the same output")
{
    const auto image = read_file(firmware("hello_world.bin"));
    std::vector<std::string> runs;
    for (int run = 0; run < 2; ++run) {
        auto machine = boot(image);
        REQUIRE(machine);
        machine->run_until_ns(1'200'000'000ull);
        std::string trace;
        for (const auto& out : machine->take_output()) {
            trace += std::to_string(out.time_ns) + ":" + std::to_string(out.port) + ":" + std::to_string(out.value) + ";";
        }
        runs.push_back(trace);
    }
    CHECK(!runs[0].empty());
    CHECK(runs[0] == runs[1]);
}

int main(int argc, char** argv)
{
    if (argc < 2) {
        std::fprintf(stderr, "usage: esp32c3_tests <repository-dir> [filter]\n");
        return 2;
    }
    repository = argv[1];
    return simnodus::test::run_all(argc > 2 ? argv[2] : "");
}
