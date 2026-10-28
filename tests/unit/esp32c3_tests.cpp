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

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
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
