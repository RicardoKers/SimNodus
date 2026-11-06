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
#include "emu/gdb_server.hpp"
#include "emu/gdb_socket.hpp"
#include "emu/wifi_air.hpp"

#include <atomic>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <thread>
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
    // From the second interval: the first line waits behind the boot log,
    // which leaves at 115200 baud (SN-254).
    for (std::size_t i = 2; i < times.size(); ++i) {
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
    lines = run_lines(*stats, 2'500'000'000ull, 0, partial2);
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
    CHECK(has(lines, "tick 2 at 552 ms")); // after the boot log at 115200 baud
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

TEST_CASE("esp32c3: the Wi-Fi starts, calibrates its PHY and scans an empty air (E-09, SN-265)")
{
    // ESP-IDF's scan example: the full PHY calibration on the first boot, the
    // station started, a scan that finds nothing (no access point yet).
    auto machine = boot(read_file(firmware("wifi_scan.bin")));
    REQUIRE(machine);
    std::string partial;
    const auto lines = run_lines(*machine, 3'200'000'000ull, 0, partial);
    CHECK(has(lines, "falling back to full calibration"));
    CHECK(has(lines, "phy_init: Saving new calibration data"));
    CHECK(!has(lines, "pll_cal exceeds")); // the RF PLL calibration ends in time
    CHECK(has(lines, "wifi:mode : sta (34:85:18:00:c3:01)"));
    CHECK(has(lines, "scan: Total APs scanned = 0"));
    CHECK(machine->take_diagnostics().empty());
    // The next boot loads the calibration it saved in NVS.
    auto again = boot(machine->flash());
    REQUIRE(again);
    std::string partial2;
    const auto lines2 = run_lines(*again, 3'200'000'000ull, 0, partial2);
    CHECK(!has(lines2, "falling back to full calibration"));
    CHECK(has(lines2, "wifi:mode : sta"));
    CHECK(has(lines2, "scan: Total APs scanned = 0"));

    // The station example looks for "SimNodus" and does not find it.
    auto station = boot(read_file(firmware("wifi_station_wpa2.bin")));
    REQUIRE(station);
    std::string partial3;
    const auto lines3 = run_lines(*station, 3'200'000'000ull, 0, partial3);
    CHECK(has(lines3, "wifi station: connect to the AP fail"));

    // MicroPython: WLAN.active(True) returns and scan() gives an empty list.
    auto python = boot(read_file(firmware("ESP32_GENERIC_C3-20260824-v1.29.0.bin")));
    REQUIRE(python);
    std::string partial4;
    run_lines(*python, 400'000'000ull, 2, partial4);
    python->send(2, "import network; w = network.WLAN(network.STA_IF); w.active(True); print('active', w.active())\r");
    auto lines4 = run_lines(*python, 1'500'000'000ull, 2, partial4);
    CHECK(has(lines4, "active True"));
    python->send(2, "print('found', w.scan())\r");
    lines4 = run_lines(*python, 5'000'000'000ull, 2, partial4);
    CHECK(has(lines4, "found []"));
}

TEST_CASE("esp32c3: the Wi-Fi MAC sends probe requests and the scan finds SimNodus's access point (E-09, SN-266)")
{
    // ESP-IDF's scan example against an open access point on channel 6.
    auto machine = boot(read_file(firmware("wifi_scan.bin")));
    REQUIRE(machine);
    WifiApConfig config;
    config.channel = 6;
    WifiAir air(config);
    air.attach(*machine);
    std::vector<Esp32C3::WifiFrame> frames;
    air.set_monitor([&frames](const Esp32C3::WifiFrame& frame) { frames.push_back(frame); });
    std::string partial;
    const auto lines = run_lines(*machine, 3'200'000'000ull, 0, partial);
    CHECK(has(lines, "scan: Total APs scanned = 1"));
    CHECK(has(lines, "SimNodus"));
    CHECK(has(lines, "WIFI_AUTH_OPEN"));
    CHECK(has(lines, "scan: Channel \t\t6"));
    CHECK(has(lines, "scan: RSSI \t\t-40"));
    // One probe request on each channel 1-11 from the station's address, the
    // channel read from the RF PLL; one probe response on channel 6.
    std::vector<int> probed;
    int responses = 0;
    int beacons = 0;
    for (const auto& frame : frames) {
        REQUIRE(frame.bytes.size() >= 24);
        if (frame.bytes[0] == 0x40) {
            probed.push_back(frame.channel);
            CHECK(std::equal(frame.bytes.begin() + 10, frame.bytes.begin() + 16, machine->wifi_address().begin()));
        } else if (frame.bytes[0] == 0x50) {
            ++responses;
            CHECK(frame.channel == 6);
        } else if (frame.bytes[0] == 0x80) {
            ++beacons;
        }
    }
    const std::vector<int> channels{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
    CHECK(probed == channels);
    CHECK(responses == 1);
    CHECK(beacons >= 30); // every 102.4 ms
    const std::array<std::uint8_t, 6> station{0x34, 0x85, 0x18, 0x00, 0xC3, 0x01};
    CHECK(machine->wifi_address() == station);
    CHECK(machine->take_diagnostics().empty());

    // The same scan twice: the same frames at the same times.
    auto second = boot(read_file(firmware("wifi_scan.bin")));
    REQUIRE(second);
    WifiAir air2(config);
    air2.attach(*second);
    std::vector<Esp32C3::WifiFrame> frames2;
    air2.set_monitor([&frames2](const Esp32C3::WifiFrame& frame) { frames2.push_back(frame); });
    second->run_until_ns(3'200'000'000ull);
    REQUIRE(frames2.size() == frames.size());
    for (std::size_t i = 0; i < frames.size(); ++i) {
        CHECK(frames2[i].bytes == frames[i].bytes);
        CHECK(frames2[i].end_ns == frames[i].end_ns);
    }

    // A WPA2 network is reported with its security (the handshake is SN-267).
    auto secured = boot(read_file(firmware("wifi_scan.bin")));
    REQUIRE(secured);
    config.channel = 1;
    config.wpa2 = true;
    WifiAir air3(config);
    air3.attach(*secured);
    std::string partial3;
    const auto lines3 = run_lines(*secured, 3'200'000'000ull, 0, partial3);
    CHECK(has(lines3, "WIFI_AUTH_WPA2_PSK"));
    CHECK(has(lines3, "WIFI_CIPHER_TYPE_CCMP"));
    CHECK(has(lines3, "scan: Channel \t\t1"));

    // MicroPython's scan() lists it.
    auto python = boot(read_file(firmware("ESP32_GENERIC_C3-20260824-v1.29.0.bin")));
    REQUIRE(python);
    config = WifiApConfig{};
    config.channel = 11;
    WifiAir air4(config);
    air4.attach(*python);
    std::string partial4;
    run_lines(*python, 400'000'000ull, 2, partial4);
    python->send(2, "import network; w = network.WLAN(network.STA_IF); w.active(True); print(w.scan())\r");
    const auto lines4 = run_lines(*python, 5'000'000'000ull, 2, partial4);
    CHECK(has(lines4, "[(b'SimNodus', b'\\x02SN\\x00\\x00\\x01', 11, -40, 0, False)]"));

    // The station example finds the network and starts to authenticate; the
    // access point does not answer yet (SN-267), so the attempt times out.
    auto station_board = boot(read_file(firmware("wifi_station_open.bin")));
    REQUIRE(station_board);
    config = WifiApConfig{};
    config.ssid = "SimNodus-open";
    WifiAir air5(config);
    air5.attach(*station_board);
    bool authentication = false;
    air5.set_monitor([&authentication, &config](const Esp32C3::WifiFrame& frame) {
        if (frame.bytes.size() >= 30 && frame.bytes[0] == 0xB0
            && std::equal(config.bssid.begin(), config.bssid.end(), frame.bytes.begin() + 4)) {
            authentication = true; // open system, sequence 1
            CHECK(frame.bytes[24] == 0);
            CHECK(frame.bytes[26] == 1);
        }
    });
    std::string partial5;
    const auto lines5 = run_lines(*station_board, 2'500'000'000ull, 0, partial5);
    CHECK(authentication);
    CHECK(has(lines5, "wifi:state: init -> auth (0xb0)"));
}

TEST_CASE("esp32c3: a station joins the open access point and gets an address by DHCP (E-09, SN-267)")
{
    WifiApConfig config;
    config.ssid = "SimNodus-open";
    // ESP-IDF's station example.
    auto board = boot(read_file(firmware("wifi_station_open.bin")));
    REQUIRE(board);
    WifiAir air(config);
    air.attach(*board);
    std::string partial;
    const auto lines = run_lines(*board, 3'000'000'000ull, 0, partial);
    CHECK(has(lines, "wifi:state: auth -> assoc (0x0)"));
    CHECK(has(lines, "wifi:state: assoc -> run (0x10)"));
    CHECK(has(lines, "wifi:connected with SimNodus-open, aid = 1, channel 6, BW20, bssid = 02:53:4e:00:00:01"));
    CHECK(has(lines, "sta ip: 192.168.4.2, mask: 255.255.255.0, gw: 192.168.4.1"));
    CHECK(has(lines, "wifi station: connected to ap SSID:SimNodus-open"));
    CHECK(air.associated(board->wifi_address()));
    const auto lease = air.network().lease_of(board->wifi_address());
    REQUIRE(lease);
    CHECK((*lease == Ipv4Address{192, 168, 4, 2}));

    // MicroPython's WLAN.connect().
    auto python = boot(read_file(firmware("ESP32_GENERIC_C3-20260824-v1.29.0.bin")));
    REQUIRE(python);
    WifiAir air2(config);
    air2.attach(*python);
    std::string partial2;
    run_lines(*python, 400'000'000ull, 2, partial2);
    python->send(2, "import network; w = network.WLAN(network.STA_IF); w.active(True); w.connect('SimNodus-open')\r");
    run_lines(*python, 6'000'000'000ull, 2, partial2);
    python->send(2, "print(w.isconnected(), w.ifconfig())\r");
    const auto lines2 = run_lines(*python, 6'500'000'000ull, 2, partial2);
    CHECK(has(lines2, "True ('192.168.4.2', '255.255.255.0', '192.168.4.1', '192.168.4.1')"));
}

namespace {

// An IPv4 packet from 192.168.4.2 to `dst` with `protocol` and `data`.
EtherPacket ipv4_packet(const Ipv4Address& dst, std::uint8_t protocol, const std::vector<std::uint8_t>& data)
{
    EtherPacket p;
    p.to = {0x02, 0x53, 0x4E, 0x00, 0x00, 0x01};
    p.from = {0x34, 0x85, 0x18, 0x00, 0xC3, 0x01};
    p.ethertype = 0x0800;
    p.payload = {0x45, 0, 0, static_cast<std::uint8_t>(20 + data.size()), 0, 1, 0, 0, 64, protocol, 0, 0,
        192, 168, 4, 2, dst[0], dst[1], dst[2], dst[3]};
    p.payload.insert(p.payload.end(), data.begin(), data.end());
    return p;
}

} // namespace

TEST_CASE("esp32c3: the access point's network answers ping, DNS and a DHCP renewal (E-09, SN-267)")
{
    WifiNetwork network({0x02, 0x53, 0x4E, 0x00, 0x00, 0x01});
    // Ping to the gateway: an echo reply with a valid checksum.
    std::vector<std::uint8_t> echo{8, 0, 0, 0, 0x12, 0x34, 0, 1, 'h', 'i'};
    const std::uint16_t sum = internet_checksum(echo.data(), echo.size());
    echo[2] = static_cast<std::uint8_t>(sum >> 8);
    echo[3] = static_cast<std::uint8_t>(sum);
    auto out = network.handle(ipv4_packet(WifiNetwork::gateway, 1, echo));
    REQUIRE(out.size() == 1);
    const std::vector<std::uint8_t>& reply = out[0].payload;
    REQUIRE(reply.size() == 30);
    CHECK(reply[9] == 1);
    CHECK(reply[16] == 192);
    CHECK(reply[19] == 2); // to 192.168.4.2
    CHECK(reply[20] == 0); // echo reply
    CHECK(internet_checksum(reply.data(), 20) == 0);
    CHECK(internet_checksum(reply.data() + 20, 10) == 0);
    CHECK(reply[29] == 'i');
    // Ping elsewhere: no answer (no gateway to the computer yet).
    CHECK(network.handle(ipv4_packet({8, 8, 8, 8}, 1, echo)).empty());

    // DNS: "no such name", with the question repeated.
    std::vector<std::uint8_t> dns{0, 53, 0, 53, 0, 0, 0, 0, 0xAB, 0xCD, 0x01, 0x00, 0, 1, 0, 0, 0, 0, 0, 0,
        4, 't', 'e', 's', 't', 0, 0, 1, 0, 1};
    dns[0] = 0xC0;
    dns[1] = 0x00; // source port 49152
    dns[5] = static_cast<std::uint8_t>(dns.size());
    out = network.handle(ipv4_packet(WifiNetwork::gateway, 17, dns));
    REQUIRE(out.size() == 1);
    const std::vector<std::uint8_t>& answer = out[0].payload;
    REQUIRE(answer.size() == 20 + dns.size());
    CHECK(answer[20] == 0);
    CHECK(answer[21] == 53);
    CHECK(answer[22] == 0xC0); // back to the asking port
    CHECK(answer[28] == 0xAB);
    CHECK(answer[29] == 0xCD);
    CHECK(answer[30] == 0x81);       // response, recursion desired
    CHECK((answer[31] & 0x0F) == 3); // NXDOMAIN
    CHECK(answer[33] == 1);          // one question
    CHECK(answer[35] == 0);          // no answers

    // DHCP: DISCOVER -> OFFER, REQUEST of another address -> NAK.
    const auto dhcp = [](std::uint8_t type, std::optional<Ipv4Address> requested) {
        std::vector<std::uint8_t> m(240, 0);
        m[0] = 1;
        m[1] = 1;
        m[2] = 6;
        m[4] = 0x11; // xid
        const std::uint8_t chaddr[6] = {0x34, 0x85, 0x18, 0x00, 0xC3, 0x01};
        std::copy(chaddr, chaddr + 6, m.begin() + 28);
        m[236] = 99;
        m[237] = 130;
        m[238] = 83;
        m[239] = 99;
        m.insert(m.end(), {53, 1, type});
        if (requested) {
            m.insert(m.end(), {50, 4, (*requested)[0], (*requested)[1], (*requested)[2], (*requested)[3]});
        }
        m.push_back(255);
        std::vector<std::uint8_t> u{0, 68, 0, 67, static_cast<std::uint8_t>((8 + m.size()) >> 8), static_cast<std::uint8_t>(8 + m.size()), 0, 0};
        u.insert(u.end(), m.begin(), m.end());
        auto p = ipv4_packet({255, 255, 255, 255}, 17, u);
        p.payload[2] = static_cast<std::uint8_t>(p.payload.size() >> 8);
        p.payload[3] = static_cast<std::uint8_t>(p.payload.size());
        return p;
    };
    out = network.handle(dhcp(1, std::nullopt));
    REQUIRE(out.size() == 1);
    CHECK(out[0].payload[28 + 16] == 192);
    CHECK(out[0].payload[28 + 19] == 2);     // yiaddr 192.168.4.2
    CHECK(out[0].payload[28 + 240 + 2] == 2); // OFFER
    out = network.handle(dhcp(3, Ipv4Address{192, 168, 4, 9}));
    REQUIRE(out.size() == 1);
    CHECK(out[0].payload[28 + 240 + 2] == 6); // NAK
    CHECK(!network.lease_of({0x34, 0x85, 0x18, 0x00, 0xC3, 0x01}));
    out = network.handle(dhcp(3, Ipv4Address{192, 168, 4, 2}));
    REQUIRE(out.size() == 1);
    CHECK(out[0].payload[28 + 240 + 2] == 5); // ACK
    CHECK(network.lease_of({0x34, 0x85, 0x18, 0x00, 0xC3, 0x01}));
}

TEST_CASE("esp32c3: Wi-Fi frames in a pcap file (E-09, SN-266)")
{
    const fs::path path = fs::temp_directory_path() / "simnodus_wifi_test.pcap";
    {
        WifiPcap pcap;
        REQUIRE(pcap.open(path.string()));
        Esp32C3::WifiFrame frame;
        frame.bytes = {0x80, 0x00, 0x00, 0x00};
        frame.end_ns = 1'000'002'000ull;
        pcap.write(frame);
    }
    const auto bytes = read_file(path);
    fs::remove(path);
    REQUIRE(bytes.size() == 24 + 16 + 4);
    CHECK(bytes[0] == 0xD4);
    CHECK(bytes[3] == 0xA1);
    CHECK(bytes[20] == 105); // LINKTYPE_IEEE802_11
    CHECK(bytes[24] == 1);   // 1 s
    CHECK(bytes[28] == 2);   // 2 us
    CHECK(bytes[32] == 4);   // captured length
    CHECK(bytes[40] == 0x80);
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
        run_lines(*machine, 400'000'000ull, 0, partial); // the boot log leaves at 115200 baud
        machine->send(0, fault.key);
        const auto lines = run_lines(*machine, 1'000'000'000ull, 0, partial);
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

TEST_CASE("esp32c3: continuous ADC with GDMA and the temperature sensor (example 49)")
{
    // SN-247: ESP-IDF's continuous ADC driver samples GPIO0 and GPIO1 at
    // 20 kHz through the GDMA ring; its temperature sensor reads the chip's
    // temperature in the range the driver chooses.
    auto machine = boot(read_file(repository / "examples" / "firmware" / "esp32c3_adc_dma" / "esp32c3_adc_dma.bin"));
    REQUIRE(machine);
    machine->set_analog_source([](int gpio, std::uint64_t time_ns) -> std::optional<double> {
        if (gpio == 0) { // 50 Hz, 0.25 V to 2.25 V
            return 1.25 + std::sin(2 * 3.14159265358979 * 50.0 * static_cast<double>(time_ns) * 1e-9);
        }
        return gpio == 1 ? std::optional<double>(1.0) : std::nullopt;
    });
    struct Reading {
        int samples[2] = {0, 0};
        int min_mv[2] = {0, 0};
        int max_mv[2] = {0, 0};
        int mean_mv[2] = {0, 0};
        double celsius = -100;
    };
    std::string partial;
    std::uint64_t t = 0;
    auto second = [&] {
        t += 1'000'000'000ull;
        Reading r;
        for (const Line& line : run_lines(*machine, t, 0, partial)) {
            // "GPIO0: 10016 samples, min 254 mV, max 2219 mV, mean 1244 mV" or "Chip temperature: 25.0 C"
            std::string spaced = line.text;
            for (char& c : spaced) {
                c = (std::isdigit(static_cast<unsigned char>(c)) || c == '-' || c == '.') ? c : ' ';
            }
            std::istringstream words(spaced);
            std::vector<double> n;
            for (double x = 0; words >> x;) {
                n.push_back(x);
            }
            if (line.text.rfind("GPIO", 0) == 0 && n.size() == 5 && (n[0] == 0 || n[0] == 1)) {
                const int ch = static_cast<int>(n[0]);
                r.samples[ch] = static_cast<int>(n[1]);
                r.min_mv[ch] = static_cast<int>(n[2]);
                r.max_mv[ch] = static_cast<int>(n[3]);
                r.mean_mv[ch] = static_cast<int>(n[4]);
            } else if (line.text.rfind("Chip temperature", 0) == 0 && n.size() == 1) {
                r.celsius = n[0];
            }
        }
        return r;
    };
    second();
    const Reading r = second();
    std::printf("  GPIO0: %d samples, %d..%d mV, mean %d; GPIO1: %d samples, mean %d mV; %.1f C\n", r.samples[0],
        r.min_mv[0], r.max_mv[0], r.mean_mv[0], r.samples[1], r.mean_mv[1], r.celsius);
    for (int ch = 0; ch < 2; ++ch) {
        CHECK(std::abs(r.samples[ch] - 10000) <= 64); // 10 kHz each, read in frames of 32
    }
    CHECK(std::abs(r.min_mv[0] - 250) <= 10);
    CHECK(std::abs(r.max_mv[0] - 2250) <= 45); // the calibration reads 1.7 % low near 2.4 V
    CHECK(std::abs(r.mean_mv[0] - 1250) <= 15);
    CHECK(std::abs(r.mean_mv[1] - 1000) <= 10);
    CHECK(r.min_mv[1] == r.max_mv[1]);
    CHECK(std::abs(r.celsius - 25.0) < 0.5);
    // Outside the -10..80 °C range the driver moves the sensor's range.
    for (const double celsius : {-25.0, 105.0, 40.0}) {
        machine->set_chip_celsius(celsius);
        second();
        const double read = second().celsius;
        std::printf("  chip at %.0f C reads %.1f C\n", celsius, read);
        // ESP-IDF truncates to whole degrees after steps of 0.4386 degrees.
        CHECK(read <= celsius + 0.5);
        CHECK(read > celsius - 1.5);
    }
}

// esptool's SYNC command as a SLIP frame.
static std::string sync_frame()
{
    std::string frame;
    for (const int b : {0xC0, 0x00, 0x08, 0x24, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07, 0x07, 0x12, 0x20}) {
        frame += static_cast<char>(b);
    }
    frame += std::string(32, static_cast<char>(0x55));
    frame += static_cast<char>(0xC0);
    return frame;
}

// The start of the ROM's answer to SYNC: a response (1) to command 0x08.
static const std::string sync_answer{static_cast<char>(0xC0), static_cast<char>(0x01), static_cast<char>(0x08)};

// What the ROM sent on `port`, as raw bytes.
static std::string output_bytes(Esp32C3& machine, int port)
{
    std::string bytes;
    for (const auto& out : machine.take_output()) {
        if (out.port == port) {
            bytes += static_cast<char>(out.value);
        }
    }
    return bytes;
}

TEST_CASE("esp32c3: download mode answers esptool's SYNC on UART0 and USB Serial/JTAG")
{
    for (const int port : {0, 2}) {
        std::string error;
        const auto rom = read_elf(repository / "third_party" / "esp-rom-elfs" / "esp32c3_rev3_rom.elf", error, ElfMachine::Riscv);
        Esp32C3::Options options;
        options.strapping = 0x4; // GPIO9 low: download mode
        Esp32C3 machine(options);
        REQUIRE(rom && machine.load_rom(*rom, error));
        machine.reset();
        machine.run_until_ns(50'000'000);
        CHECK(output_bytes(machine, port).find("waiting for download") != std::string::npos);
        // esptool sends SYNC every 100 ms or so until an answer comes; UART0
        // first detects the baud rate from the edges (autobaud).
        std::string answer;
        for (int attempt = 0; attempt < 10 && answer.find(sync_answer) == std::string::npos; ++attempt) {
            machine.send(port, sync_frame());
            machine.run_until_ns(machine.time_ns() + 100'000'000);
            answer += output_bytes(machine, port);
        }
        CHECK(answer.find(sync_answer) != std::string::npos);
        CHECK(machine.cpu().state() != RvState::Halted);
    }
}

TEST_CASE("esp32c3: EN held low stops the chip; GPIO9 low at its rise starts download mode")
{
    auto machine = boot(read_file(firmware("hello_world.bin")));
    REQUIRE(machine);
    std::string partial;
    auto lines = run_lines(*machine, 300'000'000, 0, partial);
    CHECK(has(lines, "boot:0xc (SPI_FAST_FLASH_BOOT)"));
    machine->set_chip_enable(false);
    machine->set_boot_low(true);
    lines = run_lines(*machine, 400'000'000, 0, partial);
    CHECK(lines.empty());
    CHECK(machine->time_ns() >= 400'000'000);
    machine->set_chip_enable(true);
    lines = run_lines(*machine, 450'000'000, 0, partial);
    CHECK(has(lines, "boot:0x4 (DOWNLOAD(USB/UART0/1))"));
    CHECK(has(lines, "waiting for download"));
    CHECK(machine->download_boots() == 1);
    machine->set_boot_low(false);
    machine->set_chip_enable(false);
    machine->set_chip_enable(true);
    lines = run_lines(*machine, 800'000'000, 0, partial);
    CHECK(has(lines, "boot:0xc (SPI_FAST_FLASH_BOOT)"));
}

TEST_CASE("esp32c3: UART bytes from outside arrive one frame apart at the baud rate")
{
    auto machine = boot(read_file(firmware("hello_world.bin")));
    REQUIRE(machine);
    machine->run_until_ns(1'000'000);
    // The ROM's UART0 runs at 115200 baud, 8N1: a frame is 86.8 us.
    const std::uint64_t start = machine->time_ns();
    machine->send(0, "abcd");
    std::uint32_t count = 0;
    machine->run_until_ns(start + 80'000);
    REQUIRE(machine->bus().read(0x6000001C, 4, count));
    CHECK((count & 0x3FF) == 0);
    machine->run_until_ns(start + 180'000);
    REQUIRE(machine->bus().read(0x6000001C, 4, count));
    CHECK((count & 0x3FF) == 2);
}

TEST_CASE("esp32c3: UART0's frames on GPIO21 and from GPIO20 at the pin level")
{
    auto machine = boot(read_file(firmware("hello_world.bin")));
    REQUIRE(machine);
    machine->run_until_ns(1'000'000);
    // The ROM's first byte, 'E' (0x45), at 115200 baud: decode GPIO21's edges.
    std::vector<std::pair<std::uint64_t, bool>> edges;
    for (const auto& e : machine->take_pin_events()) {
        if (e.pin == 21 && e.state.output) {
            edges.emplace_back(e.time_ns, e.state.level);
        }
    }
    const double bit = 1e9 / 115200.0;
    std::uint64_t start = 0;
    for (const auto& [t, level] : edges) {
        if (!level) {
            start = t;
            break;
        }
    }
    REQUIRE(start != 0);
    auto level_at = [&](double t) {
        bool level = true;
        for (const auto& [at, l] : edges) {
            if (static_cast<double>(at) <= t) {
                level = l;
            }
        }
        return level;
    };
    unsigned value = 0;
    for (int i = 0; i < 8; ++i) {
        value |= (level_at(static_cast<double>(start) + (i + 1.5) * bit) ? 1u : 0u) << i;
    }
    CHECK(value == 0x45);
    CHECK(level_at(static_cast<double>(start) + 9.5 * bit)); // the stop bit
    // A frame driven on GPIO20 ('A', 0x41) lands in the RX FIFO.
    std::uint64_t t = machine->time_ns();
    machine->set_pin_input(20, true);
    t += 200'000; // idle for longer than a frame
    machine->run_until_ns(t);
    const unsigned frame = 0x200u | (0x41u << 1); // start 0, data, stop 1
    for (int i = 0; i < 10; ++i) {
        machine->set_pin_input(20, ((frame >> i) & 1) != 0);
        machine->run_until_ns(t + static_cast<std::uint64_t>((i + 1) * bit));
    }
    machine->run_until_ns(machine->time_ns() + 20'000);
    std::uint32_t status = 0;
    std::uint32_t byte = 0;
    REQUIRE(machine->bus().read(0x6000001C, 4, status));
    CHECK((status & 0x3FF) == 1);
    REQUIRE(machine->bus().read(0x60000000, 4, byte));
    CHECK(byte == 0x41);
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

TEST_CASE("esp32c3: GDB server with the RISC-V registers, breakpoints, memory, reset and detach (SN-249)")
{
    auto machine = boot(read_file(repository / "examples" / "firmware" / "esp32c3_blink" / "blink.bin"));
    REQUIRE(machine);
    const auto target = make_gdb_target(*machine);
    GdbServer server(*target);
    std::string error;
    REQUIRE(server.start(0, error));
    std::atomic<bool> stop{false};
    std::atomic<std::uint64_t> time_ns{0};
    std::string console; // UART0, read after the thread ended
    std::thread thread([&] {
        while (!stop) {
            if (server.service(5)) {
                machine->run_until_ns(machine->time_ns() + 1'000'000);
                server.after_run();
                time_ns = machine->time_ns();
            }
        }
    });
    GdbSocket client;
    REQUIRE(client.connect_client(server.port(), error));
    auto ask = [&](const std::string& packet) {
        std::string reply;
        client.send_raw(GdbSocket::frame(packet));
        return client.receive_packet(5000, reply) ? reply : std::string("<no reply>");
    };
    auto pc = [&] { // register 32, little endian
        const std::string hex = ask("p20");
        std::uint32_t value = 0;
        for (int i = 3; i >= 0 && hex.size() == 8; --i) {
            value = (value << 8) | static_cast<std::uint32_t>(std::stoul(hex.substr(static_cast<std::size_t>(2 * i), 2), nullptr, 16));
        }
        return value;
    };
    const std::string xml = ask("qXfer:features:read:target.xml:0,fff");
    CHECK(xml.find("org.gnu.gdb.riscv.cpu") != std::string::npos);
    CHECK(xml.find("riscv:rv32") != std::string::npos);
    CHECK(ask("?") == "S05");
    CHECK(ask("g").size() == 33 * 8); // x0-x31 and pc
    CHECK(ask("p346").size() == 8);   // mtvec (65 + 0x305)
    // monitor reset halt: the ROM's reset vector.
    CHECK(ask("qRcmd,72657365742068616c74").rfind("O", 0) == 0); // "reset halt": its message, then OK
    std::string ok;
    CHECK(client.receive_packet(5000, ok));
    CHECK(ok == "OK");
    CHECK(pc() == 0x40000000u); // the ROM's _start
    CHECK(ask("vCont;s:1;c") == "S05"); // gdb's stepi: the jump at the reset vector
    CHECK(pc() == 0x40001E90u);
    const std::uint64_t halted_at = time_ns;
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    CHECK(time_ns == halted_at); // halted: time stands still
    // A breakpoint at the ROM's main: continue stops there; a step moves on.
    CHECK(ask("Z0,40047e3c,2") == "OK");
    client.send_raw(GdbSocket::frame("c"));
    std::string stop_reply;
    CHECK(client.receive_packet(5000, stop_reply));
    CHECK(stop_reply == "S05");
    CHECK(pc() == 0x40047E3Cu);
    CHECK(ask("s") == "S05");
    CHECK(pc() != 0x40047E3Cu);
    CHECK(ask("z0,40047e3c,2") == "OK");
    // Memory: the ROM reads, SRAM takes writes.
    CHECK(ask("m40000400,4").size() == 8);
    CHECK(ask("M3fcd0000,4:11223344") == "OK");
    CHECK(ask("m3fcd0000,4") == "11223344");
    // Detach: the firmware runs on (the blink example's log).
    CHECK(ask("D") == "OK");
    const auto begin = std::chrono::steady_clock::now();
    while (time_ns < 600'000'000ull && std::chrono::steady_clock::now() - begin < std::chrono::seconds(30)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    stop = true;
    thread.join();
    for (const auto& out : machine->take_output()) {
        console += out.port == 0 ? std::string(1, static_cast<char>(out.value)) : std::string();
    }
    CHECK(console.find("Turning the LED") != std::string::npos);
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
