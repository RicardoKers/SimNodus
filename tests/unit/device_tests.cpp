// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// Digital devices on a board's nets (SN-203): the device models, the bus that
// resolves the nets in step with the firmware, the attachment of devices to
// boards, and a whole run with a 74HC595 driving LEDs.
#include "test_support.hpp"

#include "circuit_builder.hpp"
#include "devices/device.hpp"
#include "emu/elf.hpp"
#include "emu/stm32f103.hpp"
#include "engine_fixture.hpp"
#include "io/project_file.hpp"
#include "mcu/device_bus.hpp"
#include "sim/simulation_session.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <set>
#include <thread>

using namespace simnodus;
using namespace std::chrono_literals;

namespace {

library::ComponentLibrary& device_library()
{
    static library::ComponentLibrary library = [] {
        library::ComponentLibrary l;
        l.load_directory(SIMNODUS_TEST_LIBRARY_DIR);
        return l;
    }();
    return library;
}

const std::vector<std::string> hc595_pins{
    "DS", "SHCP", "STCP", "MR", "OE", "Q0", "Q1", "Q2", "Q3", "Q4", "Q5", "Q6", "Q7", "Q7S"};

std::uint8_t outputs(const devices::Device& d)
{
    std::uint8_t value = 0;
    for (int i = 0; i < 8; ++i) {
        const auto out = d.output(5 + static_cast<std::size_t>(i));
        value |= static_cast<std::uint8_t>((out.driven && out.high ? 1u : 0u) << i);
    }
    return value;
}

// A machine that only loops: the test writes the GPIO registers itself.
std::unique_ptr<emu::Stm32F103> idle_board()
{
    emu::ElfImage image;
    std::vector<std::uint8_t> bytes(0x24, 0);
    const std::uint32_t words[2] = {0x20005000u, 0x08000021u};
    std::memcpy(bytes.data(), words, sizeof words);
    bytes[0x20] = 0xFE; // b .
    bytes[0x21] = 0xE7;
    image.entry = 0x08000021;
    image.segments.push_back({0x08000000, 0x08000000, bytes, static_cast<std::uint32_t>(bytes.size()), true});
    auto m = std::make_unique<emu::Stm32F103>();
    std::string error;
    return m->load(image, error) ? std::move(m) : nullptr;
}

void poke(emu::Stm32F103& m, std::uint32_t address, std::uint32_t value)
{
    m.bus().write(address, 4, value);
}

constexpr std::uint32_t gpioa_crl = 0x40010800, gpioa_bsrr = 0x40010810;

} // namespace

TEST_CASE("devices: the 74HC595 shifts at SHCP, latches at STCP, clears with MR and floats with OE")
{
    std::string error;
    auto d = devices::make_device("74hc595", hc595_pins, {}, error);
    REQUIRE(d);
    CHECK(!devices::make_device("74hc999", hc595_pins, {}, error) && !error.empty());
    CHECK(!devices::make_device("74hc595", {"DS"}, {}, error));
    d->initial_input(3, true);  // MR high
    d->initial_input(4, false); // OE low
    std::uint64_t t = 0;
    auto shift = [&](bool bit) {
        d->set_input(0, bit, ++t);
        d->set_input(1, true, ++t);
        d->set_input(1, false, ++t);
    };
    for (int bit = 7; bit >= 0; --bit) {
        shift(((0xA5 >> bit) & 1) != 0);
    }
    CHECK(outputs(*d) == 0); // nothing latched yet
    CHECK(d->output(13).driven && d->output(13).high); // Q7S: the first bit shifted in (1)
    d->set_input(2, true, ++t);
    d->set_input(2, false, ++t);
    CHECK(outputs(*d) == 0xA5);
    d->set_input(4, true, ++t); // OE high: high impedance
    CHECK(!d->output(5).driven);
    CHECK(d->output(13).driven); // Q7S stays driven
    d->set_input(4, false, ++t);
    d->set_input(3, false, ++t); // MR low clears the shift register, not the latches
    CHECK(outputs(*d) == 0xA5);
    CHECK(!d->output(13).high);
    d->set_input(1, true, ++t); // no shift while MR is low
    d->set_input(1, false, ++t);
    d->set_input(3, true, ++t);
    d->set_input(2, true, ++t);
    CHECK(outputs(*d) == 0);
}

TEST_CASE("devices: the bus gives a 74HC595 the board's edges at once and the board reads Q7S back")
{
    auto m = idle_board();
    REQUIRE(m);
    mcu::DeviceRequest hc;
    hc.designator = "U2";
    hc.kind = "74hc595";
    hc.pins = hc595_pins;
    hc.supply = 3.3;
    hc.tied = {{"MR", true}, {"OE", false}};
    std::vector<mcu::DigitalNet> nets{
        {"DS", {"PA7"}, {{0, "DS"}}, 0, false},
        {"SHCP", {"PA5"}, {{0, "SHCP"}}, 0, false},
        {"STCP", {"PA4"}, {{0, "STCP"}}, 0, false},
        {"BACK", {"PA0"}, {{0, "Q7S"}}, 0, false},
    };
    mcu::DeviceBus bus;
    std::string error;
    REQUIRE(bus.init(*m, {hc}, nets, error));
    m->set_pin_hook([&](int port, int pin, std::uint64_t t) { bus.board_pin_changed(port, pin, t); });
    bus.start(0);
    CHECK(bus.owns_board_pin(0, 5));
    CHECK(!bus.owns_board_pin(0, 1));
    // PA4, PA5, PA7 push-pull outputs; PA0 floating input.
    poke(*m, gpioa_crl, (0x1u << 16) | (0x1u << 20) | (0x1u << 28) | 0x4u);
    auto clock_in = [&](bool bit) {
        poke(*m, gpioa_bsrr, bit ? (1u << 7) : (1u << 23));
        poke(*m, gpioa_bsrr, 1u << 5);
        poke(*m, gpioa_bsrr, 1u << 21);
    };
    clock_in(true);
    for (int i = 0; i < 6; ++i) {
        clock_in(false);
    }
    CHECK(!m->pin_level(0, 0)); // the 1 is in stage 6
    clock_in(false);
    CHECK(m->pin_level(0, 0)); // stage 7: Q7S drives PA0 high at once, within the same instruction
    poke(*m, gpioa_bsrr, 1u << 4); // latch
    CHECK(bus.output(0, 12).driven && bus.output(0, 12).high); // Q7
    CHECK(!bus.output(0, 5).high);                             // Q0
    const auto changes = bus.take_changes();
    CHECK(!changes.empty());
    CHECK(bus.take_messages().empty());

    // Two drivers fighting on one net: reported once.
    poke(*m, gpioa_crl, (0x1u << 16) | (0x1u << 20) | (0x1u << 28) | 0x1u); // PA0 becomes an output, low
    const auto messages = bus.take_messages();
    REQUIRE(messages.size() == 1);
    CHECK(messages[0].find("short circuit") != std::string::npos);
}

TEST_CASE("devices: a 74HC595 joins the board it is wired to, with its supply, pulls and tied pins")
{
    test::CircuitBuilder b(device_library());
    const model::Id board = b.add("bluepill", {{"G1", "0"}, {"3V3_1", "VDD"}, {"PA7", "SER"}, {"PA5", "SCK"},
        {"PA4", "LATCH"}}, {{"firmware", "firmware/shift595/shift595.elf"}});
    const model::Id chip = b.add("74hc595", {{"DS", "SER"}, {"SHCP", "SCK"}, {"STCP", "LATCH"}, {"MR", "VDD"},
        {"OE", "0"}, {"Q0", "LED0"}, {"VCC", "VDD"}, {"GND", "0"}});
    b.add("resistor", {{"1", "LATCH"}, {"2", "0"}}, {{"R", "10k"}});
    // A second chip on no board pin, and a third without supply.
    b.add("74hc595", {{"DS", "X"}, {"VCC", "VDD"}, {"GND", "0"}, {"MR", "VDD"}, {"OE", "0"}});
    b.add("74hc595", {{"DS", "SER"}, {"GND", "0"}, {"MR", "VDD"}, {"OE", "0"}});
    std::vector<sim::McuRequest> requests;
    std::vector<std::string> warnings;
    std::string error;
    sim::McuEnvironment environment;
    environment.project_dir = SIMNODUS_TEST_EXAMPLES_DIR;
    REQUIRE(sim::build_mcu_requests(b.schematic, device_library(), environment, requests, error, &warnings));
    // The chip on X has no board pin: it runs in a group of its own (SN-212).
    REQUIRE(requests.size() == 2);
    CHECK(!requests[0].options.chips_only);
    CHECK(requests[1].options.chips_only);
    REQUIRE(requests[1].options.devices.size() == 1);
    CHECK(requests[1].component == requests[1].options.devices[0].component);
    const auto& options = requests[0].options;
    REQUIRE(options.devices.size() == 2);
    const auto& first = options.devices[0];
    CHECK(first.component == chip);
    CHECK(first.supply == 3.3);
    CHECK(first.tied.at("MR") == true);
    CHECK(first.tied.at("OE") == false);
    CHECK(options.devices[1].supply == 0.0);
    bool latch_pulled_down = false;
    for (const auto& net : options.nets) {
        if (net.name == "LATCH") {
            latch_pulled_down = net.pull == -1 && net.board_pins == std::vector<std::string>{"PA4"};
        }
    }
    CHECK(latch_pulled_down);
    auto has = [&](const char* text) {
        return std::any_of(warnings.begin(), warnings.end(), [&](const std::string& w) { return w.find(text) != std::string::npos; });
    };
    CHECK(!has("not connected to a microcontroller pin"));
    CHECK(has("has no supply"));
    CHECK(has(".STCP is not connected")); // the chip without supply leaves STCP open
    (void)board;
}

TEST_CASE("devices: a 74HC595 runs a light across eight LEDs in a whole run")
{
    REQUIRE(test::engine() != nullptr);
    test::CircuitBuilder b(device_library());
    const model::Id board = b.add("bluepill", {{"G1", "0"}, {"3V3_1", "VDD"}, {"PA7", "SER"}, {"PA5", "SCK"},
        {"PA4", "LATCH"}}, {{"firmware", "firmware/shift595/shift595.elf"}});
    std::map<std::string, std::string> chip{{"DS", "SER"}, {"SHCP", "SCK"}, {"STCP", "LATCH"}, {"MR", "VDD"},
        {"OE", "0"}, {"VCC", "VDD"}, {"GND", "0"}};
    for (int i = 0; i < 8; ++i) {
        chip["Q" + std::to_string(i)] = "Q" + std::to_string(i);
        b.add("resistor", {{"1", "Q" + std::to_string(i)}, {"2", "0"}}, {{"R", "1k"}});
    }
    b.add("74hc595", chip);
    auto settings = b.schematic.settings();
    settings.speed = 0.0;
    b.schematic.set_settings(settings);
    sim::RunRequest request;
    request.schematic = b.schematic;
    request.library = &device_library();
    for (int i = 0; i < 8; ++i) {
        request.vectors.push_back("q" + std::to_string(i));
    }
    std::string error;
    sim::McuEnvironment environment;
    environment.project_dir = SIMNODUS_TEST_EXAMPLES_DIR;
    std::vector<std::string> warnings;
    REQUIRE(sim::build_mcu_requests(request.schematic, device_library(), environment, request.mcus, error, &warnings));
    CHECK(warnings.empty());
    sim::SimulationSession session;
    REQUIRE(session.start(request, error));
    const auto begin = std::chrono::steady_clock::now();
    // Step i of the light is latched at about i * 100 ms; look in the middle of each step.
    for (int step = 0; step < 8; ++step) {
        const double at = 0.05 + 0.1 * step;
        while (session.status().sim_time < at) {
            REQUIRE(session.active());
            REQUIRE(std::chrono::steady_clock::now() - begin < 120s);
            std::this_thread::sleep_for(2ms);
        }
        for (int i = 0; i < 8; ++i) {
            const double volts = session.latest("q" + std::to_string(i)).value_or(-1);
            if (i == step) {
                CHECK(volts > 3.0); // 3.3 V behind 50 ohm into 1k
            } else {
                CHECK(volts < 0.1);
            }
        }
    }
    const auto log = session.mcu_pin_log(board, "U2.Q3", 0);
    CHECK(log.changes.size() >= 2); // the device pins have logic lanes too
    for (const auto& [id, text] : session.take_mcu_diagnostics()) {
        std::printf("  diagnostic: %s\n", text.c_str());
        CHECK(false);
    }
    session.stop();
}

TEST_CASE("devices: example 25 wires its 74HC595 to the board without warnings")
{
    const auto loaded = io::load_project(std::filesystem::path(SIMNODUS_TEST_EXAMPLES_DIR) / "25_Shift_register_74HC595.snx");
    REQUIRE(loaded.schematic.has_value());
    std::vector<sim::McuRequest> requests;
    std::vector<std::string> warnings;
    std::string error;
    sim::McuEnvironment environment;
    environment.project_dir = SIMNODUS_TEST_EXAMPLES_DIR;
    REQUIRE(sim::build_mcu_requests(*loaded.schematic, device_library(), environment, requests, error, &warnings));
    for (const auto& w : warnings) {
        std::printf("  warning: %s\n", w.c_str());
    }
    CHECK(warnings.empty());
    REQUIRE(requests.size() == 1);
    REQUIRE(requests[0].options.devices.size() == 1);
    CHECK(requests[0].options.devices[0].supply == 3.3);
    CHECK(requests[0].options.devices[0].tied.size() == 3); // MR, OE, and Q7S (open)
    CHECK(requests[0].options.nets.size() == 11);           // SER, SCK, LATCH and Q0..Q7
}

namespace {

const std::vector<std::string> max7219_pins{"DIN", "CLK", "LOAD", "DOUT", "DIG0", "DIG1", "DIG2", "DIG3", "DIG4",
    "DIG5", "DIG6", "DIG7", "SEGA", "SEGB", "SEGC", "SEGD", "SEGE", "SEGF", "SEGG", "SEGDP"};

// The active digit (-1 if none) and the segment pins in A..G, DP order.
std::pair<int, std::uint8_t> scan_state(const devices::Device& d)
{
    int digit = -1;
    for (int i = 0; i < 8; ++i) {
        if (!d.output(4 + static_cast<std::size_t>(i)).high) {
            digit = i;
        }
    }
    std::uint8_t segments = 0;
    for (int s = 0; s < 8; ++s) {
        segments |= static_cast<std::uint8_t>((d.output(12 + static_cast<std::size_t>(s)).high ? 1u : 0u) << s);
    }
    return {digit, segments};
}

} // namespace

TEST_CASE("devices: the MAX7219 stores words at LOAD and scans its digits with Code B and intensity")
{
    std::string error;
    auto d = devices::make_device("max7219", max7219_pins, {}, error);
    REQUIRE(d);
    std::uint64_t t = 0;
    auto word = [&](std::uint16_t value) {
        d->set_input(2, false, ++t);
        for (int bit = 15; bit >= 0; --bit) {
            d->set_input(0, ((value >> bit) & 1) != 0, ++t);
            d->set_input(1, true, ++t);
            d->set_input(1, false, ++t);
        }
        d->set_input(2, true, ++t);
    };
    CHECK(scan_state(*d).first == -1); // shutdown at power-up: no digit active
    CHECK(d->next_event_ns() == devices::no_event);
    word(0x0B01); // scan digits 0 and 1
    word(0x0901); // Code B for digit 0
    word(0x0103); // digit 0: "3"
    word(0x0281); // digit 1: DP and G (no decode: D7 = DP, D0 = G)
    word(0x0A07); // intensity 7: lit for 15/32 of each slot
    word(0x0C01); // leave shutdown: the scan starts
    auto state = scan_state(*d);
    CHECK(state.first == 0);
    CHECK(state.second == 0x4F); // A, B, C, D, G
    const std::uint64_t slot = 156250;
    // Intensity 7 lights a segment for 15/32 of the slot: here as 15/32 of the drive for the whole slot.
    CHECK(d->output(12).strength == 15.0 / 32.0);
    CHECK(d->next_event_ns() - t == slot);
    d->advance(d->next_event_ns());
    state = scan_state(*d);
    CHECK(state.first == 1);
    CHECK(state.second == 0xC0); // G and DP
    d->advance(d->next_event_ns());
    CHECK(scan_state(*d).first == 0); // back to digit 0 (scan limit 1)
    word(0x0F01);                     // display test: every segment of eight digits, full drive
    CHECK(scan_state(*d).second == 0xFF);
    CHECK(d->output(12).strength == 1.0);
    CHECK(d->output(3).driven); // DOUT
}

TEST_CASE("devices: an STM32CubeIDE HAL project draws a heart on a MAX7219 over SPI1")
{
    std::string error;
    const auto image = emu::read_elf(
        std::filesystem::path(SIMNODUS_TEST_EXAMPLES_DIR) / "firmware/hal_spi_max7219/hal_spi_max7219.elf", error);
    REQUIRE(image.has_value());
    emu::Stm32F103 m;
    REQUIRE(m.load(*image, error));
    mcu::DeviceRequest chip;
    chip.designator = "U2";
    chip.kind = "max7219";
    chip.pins = max7219_pins;
    chip.supply = 5.0;
    std::vector<mcu::DigitalNet> nets{
        {"DIN", {"PA7"}, {{0, "DIN"}}, 0, false},
        {"CLK", {"PA5"}, {{0, "CLK"}}, 0, false},
        {"LOAD", {"PA4"}, {{0, "LOAD"}}, 0, false},
    };
    mcu::DeviceBus bus;
    REQUIRE(bus.init(m, {chip}, nets, error));
    m.set_pin_hook([&](int port, int pin, std::uint64_t at) { bus.board_pin_changed(port, pin, at); });
    bus.start(0);
    auto run_to = [&](std::uint64_t end) {
        while (m.time_ns() < end) {
            m.run_until_ns(std::min(end, std::max(bus.next_event_ns(), m.time_ns() + 1)));
            bus.advance(m.time_ns());
        }
    };
    run_to(500000000); // the heart is up after the first few hundred microseconds
    const std::uint8_t heart[8] = {0x00, 0x66, 0xFF, 0xFF, 0xFF, 0x7E, 0x3C, 0x18};
    std::set<int> rows_seen;
    for (int step = 0; step < 64; ++step) {
        run_to(std::max(bus.next_event_ns(), m.time_ns() + 1));
        int digit = -1;
        std::uint8_t segments = 0;
        for (int i = 0; i < 8; ++i) {
            if (!bus.output(0, 4 + static_cast<std::size_t>(i)).high) {
                digit = i;
            }
        }
        for (int s = 0; s < 8; ++s) {
            segments |= static_cast<std::uint8_t>((bus.output(0, 12 + static_cast<std::size_t>(s)).high ? 1u : 0u) << s);
        }
        if (digit < 0 || segments == 0) {
            continue; // between slots, or the dark part of a slot
        }
        // No decode: D6..D0 are segments A..G, D7 is DP.
        std::uint8_t expected = 0;
        for (int s = 0; s < 7; ++s) {
            expected |= static_cast<std::uint8_t>(((heart[digit] >> (6 - s)) & 1u) << s);
        }
        expected |= heart[digit] & 0x80u;
        CHECK(segments == expected);
        rows_seen.insert(digit);
    }
    CHECK(rows_seen.size() == 7); // every row but the empty first one
    CHECK(m.take_diagnostics().empty());
}

TEST_CASE("devices: example 26 wires its MAX7219 to SPI1 and a 5 V supply without warnings")
{
    const auto loaded = io::load_project(std::filesystem::path(SIMNODUS_TEST_EXAMPLES_DIR) / "26_LED_matrix_MAX7219_SPI.snx");
    REQUIRE(loaded.schematic.has_value());
    std::vector<sim::McuRequest> requests;
    std::vector<std::string> warnings;
    std::string error;
    sim::McuEnvironment environment;
    environment.project_dir = SIMNODUS_TEST_EXAMPLES_DIR;
    REQUIRE(sim::build_mcu_requests(*loaded.schematic, device_library(), environment, requests, error, &warnings));
    for (const auto& w : warnings) {
        std::printf("  warning: %s\n", w.c_str());
    }
    CHECK(warnings.empty());
    REQUIRE(requests.size() == 1);
    REQUIRE(requests[0].options.devices.size() == 1);
    CHECK(requests[0].options.devices[0].supply == 5.0);
    CHECK(requests[0].options.nets.size() == 19); // DIN, CLK, LOAD, eight rows, eight columns
    for (const auto& net : requests[0].options.nets) {
        CHECK(!net.circuit); // the MAX7219 always drives its rows and columns
    }
}

TEST_CASE("devices: an STM32CubeIDE HAL project uses a 24C02, an LM75 and a PCF8574 on I2C1")
{
    std::string error;
    const auto image = emu::read_elf(
        std::filesystem::path(SIMNODUS_TEST_EXAMPLES_DIR) / "firmware/hal_i2c_devices/hal_i2c_devices.elf", error);
    REQUIRE(image.has_value());
    emu::Stm32F103 m;
    REQUIRE(m.load(*image, error));
    auto chip = [](const char* designator, const char* kind, std::vector<std::string> pins,
                    std::map<std::string, std::string> parameters = {}) {
        mcu::DeviceRequest d;
        d.designator = designator;
        d.kind = kind;
        d.pins = std::move(pins);
        d.parameters = std::move(parameters);
        d.supply = 3.3;
        d.tied = {{"A0", false}, {"A1", false}, {"A2", false}, {"WP", false}};
        return d;
    };
    std::vector<mcu::DeviceRequest> chips{
        chip("U2", "24c02", {"SCL", "SDA", "A0", "A1", "A2", "WP"}),
        chip("U3", "lm75", {"SCL", "SDA", "A0", "A1", "A2", "OS"}, {{"temperature", "-12.5"}}),
        chip("U4", "pcf8574", {"SCL", "SDA", "A0", "A1", "A2", "INT", "P0", "P1", "P2", "P3", "P4", "P5", "P6", "P7"}),
    };
    std::vector<mcu::DigitalNet> nets{
        {"SCL", {"PB6"}, {{0, "SCL"}, {1, "SCL"}, {2, "SCL"}}, 1, false},
        {"SDA", {"PB7"}, {{0, "SDA"}, {1, "SDA"}, {2, "SDA"}}, 1, false},
    };
    for (int i = 0; i < 8; ++i) {
        nets.push_back({"P" + std::to_string(i), {}, {{2, "P" + std::to_string(i)}}, 0, true});
    }
    mcu::DeviceBus bus;
    REQUIRE(bus.init(m, chips, nets, error));
    m.set_pin_hook([&](int port, int pin, std::uint64_t at) { bus.board_pin_changed(port, pin, at); });
    bus.start(0);
    std::string text;
    auto run_to = [&](std::uint64_t end) {
        m.run_until_ns(end);
        for (const auto& b : m.take_uart_bytes()) {
            text += static_cast<char>(b.value);
        }
    };
    run_to(300000000);
    CHECK(text.find("EEPROM: SimNodus") != std::string::npos);
    CHECK(text.find("Temperature: -12.5 C") != std::string::npos);
    CHECK(text.find("no answer") == std::string::npos);
    CHECK(bus.output(2, 6).pull_up);  // P0: LED off (weak high)
    // Press the button on P4 (the circuit pulls the net low): LED 1 on P0 follows.
    bus.set_circuit_level(2, 10, false, m.time_ns());
    run_to(400000000);
    CHECK(text.find("Buttons: 1---") != std::string::npos);
    CHECK(bus.output(2, 6).driven && !bus.output(2, 6).high); // P0 pulled low: the LED lights
    for (const auto& d : m.take_diagnostics()) {
        std::printf("  diagnostic: %s\n", d.text.c_str());
        CHECK(false);
    }
    m.set_pin_hook(nullptr);
}

TEST_CASE("devices: without pull-up resistors I2C stalls and the message names the cause")
{
    std::string error;
    const auto image = emu::read_elf(
        std::filesystem::path(SIMNODUS_TEST_EXAMPLES_DIR) / "firmware/hal_i2c_devices/hal_i2c_devices.elf", error);
    REQUIRE(image.has_value());
    emu::Stm32F103 m;
    REQUIRE(m.load(*image, error));
    mcu::DeviceRequest sensor;
    sensor.designator = "U3";
    sensor.kind = "lm75";
    sensor.pins = {"SCL", "SDA", "A0", "A1", "A2", "OS"};
    sensor.supply = 3.3;
    sensor.tied = {{"A0", false}, {"A1", false}, {"A2", false}};
    std::vector<mcu::DigitalNet> nets{
        {"SCL", {"PB6"}, {{0, "SCL"}}, 0, false}, // no pull-up
        {"SDA", {"PB7"}, {{0, "SDA"}}, 0, false},
    };
    mcu::DeviceBus bus;
    REQUIRE(bus.init(m, {sensor}, nets, error));
    m.set_pin_hook([&](int port, int pin, std::uint64_t at) { bus.board_pin_changed(port, pin, at); });
    bus.start(0);
    m.run_until_ns(200000000);
    std::string text;
    for (const auto& b : m.take_uart_bytes()) {
        text += static_cast<char>(b.value);
    }
    CHECK(text.find("Temperature") == std::string::npos);
    bool named = false;
    for (const auto& d : m.take_diagnostics()) {
        named = named || d.text.find("pull-up") != std::string::npos;
    }
    CHECK(named);
    m.set_pin_hook(nullptr);
}

TEST_CASE("devices: example 27 talks to three I2C chips and reads its buttons through the circuit")
{
    REQUIRE(test::engine() != nullptr);
    const auto loaded = io::load_project(std::filesystem::path(SIMNODUS_TEST_EXAMPLES_DIR) / "27_I2C_EEPROM_sensor_expander.snx");
    REQUIRE(loaded.schematic.has_value());
    sim::RunRequest request;
    request.schematic = *loaded.schematic;
    auto settings = request.schematic.settings();
    settings.speed = 0.0;
    request.schematic.set_settings(settings);
    request.library = &device_library();
    request.vectors = {"p0", "p4"};
    model::Id board = model::no_id;
    model::Id button = model::no_id;
    model::Id eeprom = model::no_id;
    for (const auto& c : request.schematic.components()) {
        board = c.type == "bluepill" ? c.id : board;
        button = c.designator == "SW1" ? c.id : button;
        eeprom = c.designator == "U2" ? c.id : eeprom;
    }
    // The EEPROM starts with a contents file (SN-208): "Hi" at address 0.
    const auto contents = std::filesystem::temp_directory_path() / ("simnodus-eeprom-" + std::to_string(std::rand()) + ".bin");
    {
        std::ofstream out(contents, std::ios::binary);
        out << "Hi";
    }
    auto* chip = request.schematic.component(eeprom);
    REQUIRE(chip != nullptr);
    auto changed = *chip;
    changed.parameters["contents"] = contents.string();
    request.schematic.put(changed);
    std::string error;
    std::vector<std::string> warnings;
    sim::McuEnvironment environment;
    environment.project_dir = SIMNODUS_TEST_EXAMPLES_DIR;
    REQUIRE(sim::build_mcu_requests(request.schematic, device_library(), environment, request.mcus, error, &warnings));
    for (const auto& w : warnings) {
        std::printf("  warning: %s\n", w.c_str());
    }
    CHECK(warnings.empty());
    REQUIRE(request.mcus.size() == 1);
    const auto& options = request.mcus[0].options;
    CHECK(options.devices.size() == 3);
    std::filesystem::remove(contents);
    for (const auto& d : options.devices) {
        if (d.component == eeprom) {
            REQUIRE(d.memory.size() >= 2);
            CHECK(d.memory[0] == 'H');
        }
    }
    for (const auto& net : options.nets) {
        if (net.name == "SCL" || net.name == "SDA") {
            CHECK(net.pull == 1);   // the 4.7 kOhm resistors
            CHECK(!net.circuit);
        } else if (net.name == "P4") {
            CHECK(net.circuit);     // read through the circuit
        }
    }
    sim::SimulationSession session;
    REQUIRE(session.start(request, error));
    const auto begin = std::chrono::steady_clock::now();
    auto wait_for = [&](double t) {
        while (session.status().sim_time < t) {
            REQUIRE(session.active());
            REQUIRE(std::chrono::steady_clock::now() - begin < 120s);
            std::this_thread::sleep_for(2ms);
        }
    };
    std::string text;
    wait_for(0.6);
    session.mcu_uart_received(board, "usart1", 0, text);
    CHECK(text.find("EEPROM: SimNodus") != std::string::npos);
    CHECK(text.find("Temperature: 23.5 C") != std::string::npos);
    CHECK(session.latest("p0").value_or(0) > 2.0); // LED 1 off: P0 weakly high
    // The viewer's contents: the file's "Hi", the firmware's "SimNodus" at 0x10, the rest erased.
    auto memory = session.device_memory(eeprom);
    REQUIRE(memory.has_value());
    REQUIRE(memory->size() == 256);
    CHECK(std::string(memory->begin(), memory->begin() + 2) == "Hi");
    CHECK(std::string(memory->begin() + 0x10, memory->begin() + 0x18) == "SimNodus");
    CHECK((*memory)[0x20] == 0xFF);
    // New contents while running.
    session.load_device_memory(eeprom, std::vector<std::uint8_t>(256, 0x00));
    wait_for(session.status().sim_time + 0.05);
    memory = session.device_memory(eeprom);
    REQUIRE(memory.has_value());
    CHECK((*memory)[0] == 0x00);
    CHECK((*memory)[0x20] == 0x00);
    session.set_control(button, 1.0);
    wait_for(session.status().sim_time + 0.1);
    session.mcu_uart_received(board, "usart1", 0, text);
    CHECK(text.find("Buttons: 1---") != std::string::npos);
    CHECK(session.latest("p0").value_or(5) < 1.0); // P0 pulled low: LED 1 lights
    for (const auto& [id, message] : session.take_mcu_diagnostics()) {
        std::printf("  diagnostic: %s\n", message.c_str());
        CHECK(false);
    }
    session.stop();
}

TEST_CASE("devices: the HD44780 takes 8-bit writes at E falling, reports busy, and maps 20x4 rows")
{
    std::string error;
    const std::vector<std::string> pins{"RS", "RW", "E", "D0", "D1", "D2", "D3", "D4", "D5", "D6", "D7", "V0"};
    auto d = devices::make_device("hd44780", pins, {{"rows", "4"}, {"columns", "20"}}, error);
    REQUIRE(d);
    std::uint64_t t = 1000000;
    auto put = [&](bool rs, std::uint8_t value) {
        d->set_input(0, rs, t);
        d->set_input(1, false, t);
        for (int b = 0; b < 8; ++b) {
            d->set_input(3 + static_cast<std::size_t>(b), ((value >> b) & 1u) != 0, t);
        }
        d->set_input(2, true, t);
        d->set_input(2, false, t); // taken now
        t += 50000;               // longer than an instruction
    };
    put(false, 0x38); // 8-bit, two lines
    put(false, 0x0C); // display on
    put(false, 0x01); // clear (1.52 ms)
    t += 2000000;
    put(false, 0x80 | 0x14); // row 3 of a 20x4 module starts at 0x14
    put(true, 'A');
    put(true, 'B');
    put(false, 0x80 | 0x40); // row 2
    put(true, 'x');
    const auto rows = d->text();
    CHECK(rows.find("AB") != std::string::npos);
    std::vector<std::string> lines;
    std::size_t start = 0;
    for (std::size_t i = 0; i <= rows.size(); ++i) {
        if (i == rows.size() || rows[i] == '\n') {
            lines.push_back(rows.substr(start, i - start));
            start = i + 1;
        }
    }
    REQUIRE(lines.size() == 4);
    CHECK(lines[0] == std::string(20, ' '));
    CHECK(lines[1].substr(0, 1) == "x");
    CHECK(lines[2].substr(0, 2) == "AB");
    // Busy flag and address: RS 0, RW 1, E high drives D7..D0.
    put(false, 0x01); // clear: busy for 1.52 ms
    t -= 50000;
    d->set_input(0, false, t);
    d->set_input(1, true, t);
    d->set_input(2, true, t);
    CHECK(d->output(10).driven && d->output(10).high); // D7: busy
    d->set_input(2, false, t);
    CHECK(!d->output(10).driven);
    // A write while busy is ignored and reported once.
    d->set_input(1, false, t);
    put(true, 'Z');
    CHECK(d->text().find('Z') == std::string::npos);
    CHECK(d->take_messages().size() == 1);
}

TEST_CASE("devices: an STM32CubeIDE HAL project writes a 16x2 LCD through a PCF8574 backpack")
{
    std::string error;
    const auto image = emu::read_elf(
        std::filesystem::path(SIMNODUS_TEST_EXAMPLES_DIR) / "firmware/hal_i2c_lcd/hal_i2c_lcd.elf", error);
    REQUIRE(image.has_value());
    emu::Stm32F103 m;
    REQUIRE(m.load(*image, error));
    mcu::DeviceRequest pcf;
    pcf.designator = "U2";
    pcf.kind = "pcf8574";
    pcf.pins = {"SCL", "SDA", "A0", "A1", "A2", "INT", "P0", "P1", "P2", "P3", "P4", "P5", "P6", "P7"};
    pcf.supply = 5.0;
    pcf.tied = {{"A0", true}, {"A1", true}, {"A2", true}}; // 0x27
    mcu::DeviceRequest lcd;
    lcd.designator = "LCD1";
    lcd.kind = "hd44780";
    lcd.pins = {"V0", "RS", "RW", "E", "D0", "D1", "D2", "D3", "D4", "D5", "D6", "D7", "A", "K"};
    lcd.parameters = {{"rows", "2"}, {"columns", "16"}};
    lcd.supply = 5.0;
    lcd.tied = {{"D0", false}, {"D1", false}, {"D2", false}, {"D3", false}};
    std::vector<mcu::DigitalNet> nets{
        {"SCL", {"PB6"}, {{0, "SCL"}}, 1, false},
        {"SDA", {"PB7"}, {{0, "SDA"}}, 1, false},
        {"RS", {}, {{0, "P0"}, {1, "RS"}}, 0, false},
        {"RW", {}, {{0, "P1"}, {1, "RW"}}, 0, false},
        {"E", {}, {{0, "P2"}, {1, "E"}}, 0, false},
    };
    for (int i = 4; i < 8; ++i) {
        nets.push_back({"D" + std::to_string(i), {}, {{0, "P" + std::to_string(i)}, {1, "D" + std::to_string(i)}}, 0, false});
    }
    mcu::DeviceBus bus;
    REQUIRE(bus.init(m, {pcf, lcd}, nets, error));
    m.set_pin_hook([&](int port, int pin, std::uint64_t at) { bus.board_pin_changed(port, pin, at); });
    bus.start(0);
    m.run_until_ns(2600000000ull);
    const std::string text = bus.text(1);
    CHECK(text.substr(0, 16) == "Hello, SimNodus!");
    CHECK(text.find("Seconds:") != std::string::npos);
    CHECK(text.find("Seconds:       2") != std::string::npos);
    for (const auto& message : bus.take_messages()) {
        std::printf("  message: %s\n", message.c_str());
        CHECK(false);
    }
    m.set_pin_hook(nullptr);
}

TEST_CASE("devices: examples 27 and 28 wire their chips without warnings")
{
    for (const char* file : {"27_I2C_EEPROM_sensor_expander.snx", "28_LCD_16x2_I2C_backpack.snx"}) {
        const auto loaded = io::load_project(std::filesystem::path(SIMNODUS_TEST_EXAMPLES_DIR) / file);
        REQUIRE(loaded.schematic.has_value());
        std::vector<sim::McuRequest> requests;
        std::vector<std::string> warnings;
        std::string error;
        sim::McuEnvironment environment;
        environment.project_dir = SIMNODUS_TEST_EXAMPLES_DIR;
        REQUIRE(sim::build_mcu_requests(*loaded.schematic, device_library(), environment, requests, error, &warnings));
        for (const auto& w : warnings) {
            std::printf("  %s: %s\n", file, w.c_str());
        }
        CHECK(warnings.empty());
        REQUIRE(requests.size() == 1);
        for (const auto& net : requests[0].options.nets) {
            if (net.name == "E" || net.name == "D4") {
                CHECK(!net.circuit); // a net between two chips stays digital
            }
        }
    }
}

TEST_CASE("devices: example 30 scans a 4x4 keypad held through its control")
{
    // SN-210: the keypad's control is the number of the key held; the firmware reads it through the circuit.
    REQUIRE(test::engine() != nullptr);
    const auto loaded = io::load_project(std::filesystem::path(SIMNODUS_TEST_EXAMPLES_DIR) / "30_Keypad_4x4_scanned_by_Blue_Pill.snx");
    REQUIRE(loaded.schematic.has_value());
    sim::RunRequest request;
    request.schematic = *loaded.schematic;
    auto settings = request.schematic.settings();
    settings.speed = 0.0;
    request.schematic.set_settings(settings);
    request.library = &device_library();
    model::Id board = model::no_id;
    model::Id keypad = model::no_id;
    for (const auto& c : request.schematic.components()) {
        board = c.type == "bluepill" ? c.id : board;
        keypad = c.type == "keypad_4x4" ? c.id : keypad;
    }
    std::string error;
    sim::McuEnvironment environment;
    environment.project_dir = SIMNODUS_TEST_EXAMPLES_DIR;
    REQUIRE(sim::build_mcu_requests(request.schematic, device_library(), environment, request.mcus, error));
    sim::SimulationSession session;
    REQUIRE(session.start(request, error));
    const auto begin = std::chrono::steady_clock::now();
    auto wait_for = [&](double t) {
        while (session.status().sim_time < t) {
            REQUIRE(session.active());
            REQUIRE(std::chrono::steady_clock::now() - begin < 120s);
            std::this_thread::sleep_for(2ms);
        }
    };
    std::string text;
    wait_for(0.1);
    session.mcu_uart_received(board, "usart1", 0, text);
    CHECK(text.find("Keypad ready") != std::string::npos);
    CHECK(text.find("Key:") == std::string::npos);
    // Key 6 is "5" (second row, second column), key 16 is "D".
    for (const auto& [number, shown] : std::vector<std::pair<double, std::string>>{{6, "Key: 5"}, {16, "Key: D"}}) {
        session.set_control(keypad, number);
        wait_for(session.status().sim_time + 0.1);
        session.set_control(keypad, 0);
        wait_for(session.status().sim_time + 0.05);
        session.mcu_uart_received(board, "usart1", 0, text);
        CHECK(text.find(shown) != std::string::npos);
    }
    for (const auto& [id, message] : session.take_mcu_diagnostics()) {
        std::printf("  diagnostic: %s\n", message.c_str());
        CHECK(false);
    }
    session.stop();
}

TEST_CASE("devices: parallel RAM and ROM read, write, ignore the unused address pins and float")
{
    // SN-212: pins in the order the library gives them.
    std::vector<std::string> pins;
    for (int i = 0; i < 16; ++i) {
        pins.push_back("A" + std::to_string(i));
    }
    for (int i = 0; i < 8; ++i) {
        pins.push_back("D" + std::to_string(i));
    }
    pins.insert(pins.end(), {"CE", "OE", "WE"});
    const std::size_t a0 = 0, d0 = 16, ce = 24, oe = 25, we = 26;
    std::string error;
    CHECK(!devices::make_device("ram", pins, {{"size", "100"}}, error));
    auto ram = devices::make_device("ram", pins, {{"size", "256"}}, error);
    REQUIRE(ram);
    for (std::size_t p = 0; p < pins.size(); ++p) {
        ram->initial_input(p, p >= ce); // address 0, data 0, deselected
    }
    std::uint64_t t = 1000;
    auto set_address = [&](unsigned address) {
        for (std::size_t b = 0; b < 16; ++b) {
            ram->set_input(a0 + b, ((address >> b) & 1u) != 0, t);
        }
    };
    auto set_data = [&](unsigned value) {
        for (std::size_t b = 0; b < 8; ++b) {
            ram->set_input(d0 + b, ((value >> b) & 1u) != 0, t);
        }
    };
    auto data = [&] {
        unsigned value = 0;
        for (std::size_t b = 0; b < 8; ++b) {
            const auto out = ram->output(d0 + b);
            if (!out.driven) {
                return -1;
            }
            value |= (out.high ? 1u : 0u) << b;
        }
        return static_cast<int>(value);
    };
    CHECK(data() == -1); // CE high: off
    // Write 0xA5 at 0x12: CE low, WE low, data, WE rises.
    ram->set_input(ce, false, t);
    set_address(0x12);
    ram->set_input(we, false, t);
    set_data(0xA5);
    CHECK(data() == -1); // writing: the data pins are inputs
    ram->set_input(we, true, ++t);
    CHECK(ram->memory()[0x12] == 0xA5);
    CHECK(ram->memory_version() == 1);
    // Read it back with OE low; A8 is beyond 256 bytes and ignored.
    ram->set_input(oe, false, ++t);
    CHECK(data() == 0xA5);
    set_address(0x112);
    CHECK(data() == 0xA5);
    set_address(0x13);
    CHECK(data() == 0x00); // a RAM starts with zeros

    auto rom = devices::make_device("rom", {pins.begin(), pins.begin() + 26}, {{"size", "16"}}, error);
    REQUIRE(rom);
    CHECK(rom->memory().size() == 16);
    CHECK(rom->memory()[3] == 0xFF); // erased
    rom->load_memory({0x3F, 0x06, 0x5B});
    for (std::size_t p = 0; p < 26; ++p) {
        rom->initial_input(p, false); // address 0, CE and OE low
    }
    rom->set_input(a0 + 1, true, t); // address 2
    unsigned value = 0;
    for (std::size_t b = 0; b < 8; ++b) {
        value |= (rom->output(d0 + b).high ? 1u : 0u) << b;
    }
    CHECK(value == 0x5B);
    rom->set_input(oe, true, t);
    CHECK(!rom->output(d0).driven);
}

TEST_CASE("devices: example 32 counts on a display through a ROM without a board")
{
    // SN-212: the ROM is on no board pin; it runs in a chip group, its address read from the circuit.
    REQUIRE(test::engine() != nullptr);
    const auto loaded = io::load_project(std::filesystem::path(SIMNODUS_TEST_EXAMPLES_DIR) / "32_ROM_lookup_seven_segment.snx");
    REQUIRE(loaded.schematic.has_value());
    sim::RunRequest request;
    request.schematic = *loaded.schematic;
    auto settings = request.schematic.settings();
    settings.speed = 0.0;
    request.schematic.set_settings(settings);
    request.library = &device_library();
    const std::string segments = "abcdefg";
    for (const char s : segments) {
        request.vectors.push_back(std::string(1, s));
    }
    std::string error;
    std::vector<std::string> warnings;
    sim::McuEnvironment environment;
    environment.project_dir = SIMNODUS_TEST_EXAMPLES_DIR;
    REQUIRE(sim::build_mcu_requests(request.schematic, device_library(), environment, request.mcus, error, &warnings));
    for (const auto& w : warnings) {
        std::printf("  warning: %s\n", w.c_str());
    }
    CHECK(warnings.empty());
    REQUIRE(request.mcus.size() == 1);
    CHECK(request.mcus[0].options.chips_only);
    REQUIRE(request.mcus[0].options.devices.size() == 1);
    CHECK(request.mcus[0].options.devices[0].memory.size() == 16); // the contents file
    sim::SimulationSession session;
    REQUIRE(session.start(request, error));
    const auto begin = std::chrono::steady_clock::now();
    const std::vector<int> digits{0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F, 0x77, 0x7C, 0x39, 0x5E, 0x79, 0x71};
    std::vector<int> seen;
    for (double at = 0.25; at < 3.0; at += 0.5) { // the middle of each count
        while (session.status().sim_time < at) {
            REQUIRE(session.active());
            REQUIRE(std::chrono::steady_clock::now() - begin < 120s);
            std::this_thread::sleep_for(2ms);
        }
        int value = 0;
        for (std::size_t s = 0; s < segments.size(); ++s) {
            value |= session.latest(std::string(1, segments[s])).value_or(0) > 1.0 ? 1 << s : 0;
        }
        seen.push_back(value);
    }
    // Consecutive hex digits on the display.
    REQUIRE(seen.size() == 6);
    const auto first = std::find(digits.begin(), digits.end(), seen[0]);
    REQUIRE(first != digits.end());
    for (std::size_t k = 1; k < seen.size(); ++k) {
        CHECK(seen[k] == digits[static_cast<std::size_t>(first - digits.begin() + static_cast<long>(k)) % 16]);
    }
    for (const auto& [id, message] : session.take_mcu_diagnostics()) {
        std::printf("  diagnostic: %s\n", message.c_str());
        CHECK(false);
    }
    session.stop();
}

TEST_CASE("devices: example 33 writes and reads a parallel RAM from a Blue Pill")
{
    // SN-212: the RAM on the board's pins; its contents reach the Memory Viewer's source.
    REQUIRE(test::engine() != nullptr);
    const auto loaded = io::load_project(std::filesystem::path(SIMNODUS_TEST_EXAMPLES_DIR) / "33_Parallel_RAM_on_Blue_Pill.snx");
    REQUIRE(loaded.schematic.has_value());
    sim::RunRequest request;
    request.schematic = *loaded.schematic;
    auto settings = request.schematic.settings();
    settings.speed = 0.0;
    request.schematic.set_settings(settings);
    request.library = &device_library();
    model::Id board = model::no_id;
    model::Id ram = model::no_id;
    for (const auto& c : request.schematic.components()) {
        board = c.type == "bluepill" ? c.id : board;
        ram = c.type == "ram" ? c.id : ram;
    }
    std::string error;
    std::vector<std::string> warnings;
    sim::McuEnvironment environment;
    environment.project_dir = SIMNODUS_TEST_EXAMPLES_DIR;
    REQUIRE(sim::build_mcu_requests(request.schematic, device_library(), environment, request.mcus, error, &warnings));
    for (const auto& w : warnings) {
        std::printf("  warning: %s\n", w.c_str());
    }
    CHECK(warnings.empty());
    REQUIRE(request.mcus.size() == 1);
    CHECK(!request.mcus[0].options.chips_only);
    sim::SimulationSession session;
    REQUIRE(session.start(request, error));
    const auto begin = std::chrono::steady_clock::now();
    auto wait_for = [&](double t) {
        while (session.status().sim_time < t) {
            REQUIRE(session.active());
            REQUIRE(std::chrono::steady_clock::now() - begin < 120s);
            std::this_thread::sleep_for(2ms);
        }
    };
    wait_for(0.35);
    std::string text;
    session.mcu_uart_received(board, "usart1", 0, text);
    CHECK(text.find("RAM test: 256 bytes written and read back") != std::string::npos);
    CHECK(text.find("Text at 0x00: Hello from the Blue Pill") != std::string::npos);
    const auto memory = session.device_memory(ram);
    REQUIRE(memory.has_value());
    REQUIRE(memory->size() == 256);
    CHECK(std::string(memory->begin(), memory->begin() + 24) == "Hello from the Blue Pill");
    CHECK((*memory)[0x40] == (0x40 ^ 0x5A));
    CHECK((*memory)[0x20] >= 2); // the counter, every 100 ms
    for (const auto& [id, message] : session.take_mcu_diagnostics()) {
        std::printf("  diagnostic: %s\n", message.c_str());
        CHECK(false);
    }
    session.stop();
}

TEST_CASE("devices: DIP counters and shift registers follow their datasheets (SN-222)")
{
    std::string error;
    // Index of a pin in the list given to make_device.
    auto at = [](const std::vector<std::string>& list, const std::string& name) {
        return static_cast<std::size_t>(std::find(list.begin(), list.end(), name) - list.begin());
    };
    auto high = [](const devices::Device& d, std::size_t pin) {
        const auto o = d.output(pin);
        return o.driven && o.high;
    };
    std::uint64_t t = 0;
    auto pulse = [&t](devices::Device& d, std::size_t pin) {
        d.set_input(pin, true, t += 1000);
        d.set_input(pin, false, t += 1000);
    };
    {
        const std::vector<std::string> p{"MRN", "CP", "D0", "D1", "D2", "D3", "CEP", "PEN", "CET", "Q3", "Q2", "Q1",
            "Q0", "TC"};
        auto d = devices::make_device("74hc161", p, {}, error);
        REQUIRE(d);
        for (const auto* name : {"MRN", "CEP", "PEN", "CET"}) {
            d->initial_input(at(p, name), true);
        }
        auto count = [&] {
            return (high(*d, at(p, "Q0")) ? 1 : 0) | (high(*d, at(p, "Q1")) ? 2 : 0) | (high(*d, at(p, "Q2")) ? 4 : 0)
                | (high(*d, at(p, "Q3")) ? 8 : 0);
        };
        for (int i = 0; i < 15; ++i) {
            pulse(*d, at(p, "CP"));
        }
        CHECK(count() == 15);
        CHECK(high(*d, at(p, "TC")));
        pulse(*d, at(p, "CP"));
        CHECK(count() == 0);
        d->set_input(at(p, "D1"), true, t += 1000);
        d->set_input(at(p, "D3"), true, t += 1000);
        d->set_input(at(p, "PEN"), false, t += 1000);
        CHECK(count() == 0); // loading waits for the clock
        pulse(*d, at(p, "CP"));
        CHECK(count() == 10);
        d->set_input(at(p, "PEN"), true, t += 1000);
        d->set_input(at(p, "CEP"), false, t += 1000);
        pulse(*d, at(p, "CP"));
        CHECK(count() == 10); // CEP low holds
        d->set_input(at(p, "MRN"), false, t += 1000);
        CHECK(count() == 0); // MR clears at once
        CHECK(!devices::make_device("74hc161", {"CP"}, {}, error));
    }
    {
        const std::vector<std::string> p{"D1", "Q1", "Q0", "CPD", "CPU", "Q2", "Q3", "D3", "D2", "PLN", "TCUN", "TCDN",
            "MR", "D0"};
        auto d = devices::make_device("74hc193", p, {}, error);
        REQUIRE(d);
        d->initial_input(at(p, "PLN"), true);
        d->initial_input(at(p, "CPU"), true);
        d->initial_input(at(p, "CPD"), true);
        auto count = [&] {
            return (high(*d, at(p, "Q0")) ? 1 : 0) | (high(*d, at(p, "Q1")) ? 2 : 0) | (high(*d, at(p, "Q2")) ? 4 : 0)
                | (high(*d, at(p, "Q3")) ? 8 : 0);
        };
        // Counting down from 0: borrow (TCD low while CPD is low), then 15.
        d->set_input(at(p, "CPD"), false, t += 1000);
        CHECK(!high(*d, at(p, "TCDN")));
        d->set_input(at(p, "CPD"), true, t += 1000);
        CHECK(count() == 15);
        d->set_input(at(p, "CPU"), false, t += 1000);
        CHECK(!high(*d, at(p, "TCUN"))); // carry at 15 while CPU is low
        d->set_input(at(p, "CPU"), true, t += 1000);
        CHECK(count() == 0);
        d->set_input(at(p, "D0"), true, t += 1000);
        d->set_input(at(p, "D2"), true, t += 1000);
        d->set_input(at(p, "PLN"), false, t += 1000);
        CHECK(count() == 5); // parallel load at once
        d->set_input(at(p, "PLN"), true, t += 1000);
        d->set_input(at(p, "MR"), true, t += 1000);
        CHECK(count() == 0);
    }
    {
        const std::vector<std::string> p{"PLN", "CP", "D4", "D5", "D6", "D7", "Q7N", "Q7", "DS", "D0", "D1", "D2", "D3",
            "CEN"};
        auto d = devices::make_device("74hc165", p, {}, error);
        REQUIRE(d);
        d->initial_input(at(p, "PLN"), true);
        // Load 0b10100101 (D7..D0) and read it MSB first, as SPI does.
        for (int bit : {0, 2, 5, 7}) {
            d->set_input(at(p, "D" + std::to_string(bit)), true, t += 1000);
        }
        d->set_input(at(p, "PLN"), false, t += 1000);
        d->set_input(at(p, "PLN"), true, t += 1000);
        int value = 0;
        for (int i = 0; i < 8; ++i) {
            value = (value << 1) | (high(*d, at(p, "Q7")) ? 1 : 0);
            CHECK(high(*d, at(p, "Q7N")) != high(*d, at(p, "Q7")));
            pulse(*d, at(p, "CP"));
        }
        CHECK(value == 0xA5);
    }
    {
        const std::vector<std::string> p{"Q5", "Q1", "Q0", "Q2", "Q6", "Q7", "Q3", "Q8", "Q4", "Q9", "CO", "CP1", "CP0",
            "MR"};
        auto d = devices::make_device("cd4017", p, {}, error);
        REQUIRE(d);
        CHECK(high(*d, at(p, "Q0")));
        CHECK(high(*d, at(p, "CO")));
        for (int i = 0; i < 7; ++i) {
            pulse(*d, at(p, "CP0"));
        }
        CHECK(high(*d, at(p, "Q7")));
        CHECK(!high(*d, at(p, "Q0")));
        CHECK(!high(*d, at(p, "CO")));
        d->set_input(at(p, "CP1"), true, t += 1000);
        pulse(*d, at(p, "CP0"));
        CHECK(high(*d, at(p, "Q7"))); // CP1 high inhibits
        d->set_input(at(p, "CP0"), true, t += 1000);
        d->set_input(at(p, "CP1"), false, t += 1000); // falling CP1 with CP0 high advances
        CHECK(high(*d, at(p, "Q8")));
        d->set_input(at(p, "MR"), true, t += 1000);
        CHECK(high(*d, at(p, "Q0")));
    }
}

namespace {

bool near(double value, double expected, double tolerance)
{
    const bool ok = std::abs(value - expected) <= tolerance;
    if (!ok) {
        std::printf("  got %.6g, expected %.6g +- %.3g\n", value, expected, tolerance);
    }
    return ok;
}

// Drives a device's SCL and SDA like an I2C master; SDA is the wired-AND of
// the master and the device's open drain, given back to the device on change.
class I2cMaster {
public:
    I2cMaster(devices::Device& device, std::size_t scl, std::size_t sda) : d_(device), scl_(scl), sda_(sda)
    {
        d_.initial_input(scl_, true);
        d_.initial_input(sda_, true);
    }
    std::uint64_t& time() { return t_; }
    void start()
    {
        sda(true);
        scl(true);
        sda(false);
        scl(false);
    }
    void stop()
    {
        sda(false);
        scl(true);
        sda(true);
    }
    bool write(std::uint8_t byte)
    {
        for (int i = 7; i >= 0; --i) {
            sda(((byte >> i) & 1) != 0);
            scl(true);
            scl(false);
        }
        sda(true);
        scl(true);
        const bool ack = !net_;
        scl(false);
        return ack;
    }
    std::uint8_t read(bool ack)
    {
        sda(true);
        unsigned value = 0;
        for (int i = 0; i < 8; ++i) {
            scl(true);
            value = (value << 1) | (net_ ? 1u : 0u);
            scl(false);
        }
        sda(!ack);
        scl(true);
        scl(false);
        sda(true);
        return static_cast<std::uint8_t>(value);
    }
    bool write_registers(std::uint8_t address, const std::vector<std::uint8_t>& bytes)
    {
        start();
        bool ok = write(static_cast<std::uint8_t>(address << 1));
        for (const auto b : bytes) {
            ok = write(b) && ok;
        }
        stop();
        return ok;
    }
    std::vector<std::uint8_t> read_registers(std::uint8_t address, std::uint8_t reg, std::size_t count)
    {
        start();
        write(static_cast<std::uint8_t>(address << 1));
        write(reg);
        start(); // repeated START
        std::vector<std::uint8_t> bytes;
        if (write(static_cast<std::uint8_t>((address << 1) | 1))) {
            for (std::size_t i = 0; i < count; ++i) {
                bytes.push_back(read(i + 1 < count));
            }
        }
        stop();
        return bytes;
    }

private:
    void sda(bool level)
    {
        master_ = level;
        settle();
    }
    void scl(bool level)
    {
        d_.set_input(scl_, level, t_ += 5000);
        settle();
    }
    void settle()
    {
        const auto o = d_.output(sda_);
        const bool net = master_ && !(o.driven && !o.high);
        if (net != net_) {
            net_ = net;
            d_.set_input(sda_, net, t_ += 1000);
            settle();
        }
    }
    devices::Device& d_;
    std::size_t scl_, sda_;
    std::uint64_t t_ = 0;
    bool master_ = true, net_ = true;
};

} // namespace

TEST_CASE("devices: I2C sensors, clock and OLED answer like their datasheets (SN-223)")
{
    std::string error;
    { // BMP280: the datasheet's compensation of the raw values gives the set temperature and pressure.
        auto d = devices::make_device("bmp280", {"SCL", "SDA", "SDO"}, {{"temperature", "21.5"}, {"pressure", "950"}},
            error);
        REQUIRE(d);
        I2cMaster m(*d, 0, 1);
        CHECK(m.read_registers(0x76, 0xD0, 1) == std::vector<std::uint8_t>{0x58});
        CHECK(m.read_registers(0x77, 0xD0, 1).empty()); // SDO low: 0x76 only
        const auto c = m.read_registers(0x76, 0x88, 24);
        REQUIRE(c.size() == 24);
        auto u16 = [&](int i) { return static_cast<double>(c[i] | (c[i + 1] << 8)); };
        auto s16 = [&](int i) { return static_cast<double>(static_cast<std::int16_t>(c[i] | (c[i + 1] << 8))); };
        CHECK(m.read_registers(0x76, 0xFA, 3) == (std::vector<std::uint8_t>{0x80, 0x00, 0x00}));
        CHECK(m.write_registers(0x76, {0xF4, 0x25})); // temperature and pressure x1, forced
        CHECK(m.read_registers(0x76, 0xF4, 1) == std::vector<std::uint8_t>{0x24}); // back to sleep
        const auto r = m.read_registers(0x76, 0xF7, 6);
        REQUIRE(r.size() == 6);
        const double adc_p = (r[0] << 12) | (r[1] << 4) | (r[2] >> 4);
        const double adc_t = (r[3] << 12) | (r[4] << 4) | (r[5] >> 4);
        // Floating-point compensation, datasheet section 8.1.
        double v1 = (adc_t / 16384.0 - u16(0) / 1024.0) * s16(2);
        double v2 = (adc_t / 131072.0 - u16(0) / 8192.0) * (adc_t / 131072.0 - u16(0) / 8192.0) * s16(4);
        const double fine = v1 + v2;
        CHECK(near(fine / 5120.0, 21.5, 0.02));
        v1 = fine / 2.0 - 64000.0;
        v2 = v1 * v1 * s16(16) / 32768.0 + v1 * s16(14) * 2.0;
        v2 = v2 / 4.0 + s16(12) * 65536.0;
        v1 = (s16(10) * v1 * v1 / 524288.0 + s16(8) * v1) / 524288.0;
        v1 = (1.0 + v1 / 32768.0) * u16(6);
        double p = 1048576.0 - adc_p;
        p = (p - v2 / 4096.0) * 6250.0 / v1;
        v1 = s16(22) * p * p / 2147483648.0;
        v2 = p * s16(20) / 32768.0;
        p = p + (v1 + v2 + s16(18)) / 16.0;
        CHECK(near(p, 95000.0, 3.0));
    }
    { // MPU6050: asleep at reset, then 1 g on Z at +-2 g and +-4 g, a rotation and the temperature.
        auto d = devices::make_device("mpu6050", {"SCL", "SDA", "AD0"}, {{"gyro_x", "250"}}, error);
        REQUIRE(d);
        I2cMaster m(*d, 0, 1);
        CHECK(m.read_registers(0x68, 0x75, 1) == std::vector<std::uint8_t>{0x68});
        CHECK(m.read_registers(0x68, 0x3F, 2) == (std::vector<std::uint8_t>{0, 0}));
        CHECK(m.write_registers(0x68, {0x6B, 0x00}));
        auto word = [](const std::vector<std::uint8_t>& b, std::size_t i) {
            return static_cast<std::int16_t>((b[i] << 8) | b[i + 1]);
        };
        auto s = m.read_registers(0x68, 0x3B, 14);
        REQUIRE(s.size() == 14);
        CHECK(word(s, 4) == 16384);
        CHECK(word(s, 6) == -3920); // (25 - 36.53) x 340
        CHECK(word(s, 8) == 32750); // 250 deg/s x 131
        CHECK(m.write_registers(0x68, {0x1C, 0x08}));
        s = m.read_registers(0x68, 0x3B, 6);
        CHECK(word(s, 4) == 8192);
        d->set_parameter("accel_x", "-0.5");
        s = m.read_registers(0x68, 0x3B, 2);
        CHECK(word(s, 0) == -4096);
    }
    { // DS3231: time runs with the simulation, a written time, the 1 Hz square wave and an alarm.
        auto d = devices::make_device("ds3231", {"SCL", "SDA", "SQW"}, {{"start", "2026-10-26 23:59:58"}}, error);
        REQUIRE(d);
        I2cMaster m(*d, 0, 1);
        CHECK(m.read_registers(0x68, 0x00, 7) == (std::vector<std::uint8_t>{0x58, 0x59, 0x23, 0x02, 0x26, 0x10, 0x26}));
        m.time() = 3000000000ull;
        CHECK(m.read_registers(0x68, 0x00, 7) == (std::vector<std::uint8_t>{0x01, 0x00, 0x00, 0x03, 0x27, 0x10, 0x26}));
        CHECK(m.write_registers(0x68, {0x00, 0x30, 0x45, 0x52, 0x05, 0x31, 0x12, 0x30})); // 12-hour 12:45:30 PM
        m.time() += 1000000000ull;
        CHECK(m.read_registers(0x68, 0x00, 7) == (std::vector<std::uint8_t>{0x31, 0x45, 0x52, 0x05, 0x31, 0x12, 0x30}));
        auto low = [&] {
            const auto o = d->output(2);
            return o.driven && !o.high;
        };
        CHECK(!low()); // INTCN set at reset: no square wave
        CHECK(m.write_registers(0x68, {0x0E, 0x00})); // INTCN 0, 1 Hz
        int changes = 0;
        bool last = low();
        for (int i = 0; i < 8; ++i) {
            d->advance(d->next_event_ns());
            changes += low() != last ? 1 : 0;
            last = low();
        }
        CHECK(changes == 8); // half periods of 0.5 s
        CHECK(m.write_registers(0x68, {0x07, 0x80, 0x80, 0x80, 0x80})); // alarm 1 every second
        CHECK(m.write_registers(0x68, {0x0E, 0x05})); // INTCN, A1IE
        CHECK(!low());
        d->advance(d->next_event_ns());
        d->advance(d->next_event_ns());
        CHECK(low());
        CHECK(m.write_registers(0x68, {0x0F, 0x00})); // clear A1F
        CHECK(!low());
    }
    { // TCS34725: counts from the color, the light, the gain and the integration time.
        auto d = devices::make_device("tcs34725", {"SCL", "SDA"}, {{"red", "255"}, {"green", "0"}, {"blue", "0"}}, error);
        REQUIRE(d);
        I2cMaster m(*d, 0, 1);
        CHECK(m.read_registers(0x29, 0x92, 1) == std::vector<std::uint8_t>{0x44});
        CHECK(m.write_registers(0x29, {0x80, 0x03}));
        CHECK(m.write_registers(0x29, {0x81, 0xF6})); // 10 cycles
        CHECK(m.write_registers(0x29, {0x8F, 0x01})); // gain 4
        const auto r = m.read_registers(0x29, 0xB3, 9); // STATUS, then C, R, G, B with auto-increment
        REQUIRE(r.size() == 9);
        CHECK(r[0] == 1); // AVALID
        CHECK((r[1] | (r[2] << 8)) == 1600);
        CHECK((r[3] | (r[4] << 8)) == 1600);
        CHECK((r[5] | (r[6] << 8)) == 0);
    }
    { // SSD1306: the usual initialization, a pixel in two corners, inverse, then off.
        auto d = devices::make_device("ssd1306", {"SCL", "SDA"}, {{"height", "64"}}, error);
        REQUIRE(d);
        I2cMaster m(*d, 0, 1);
        CHECK(d->text() == "128x64 off");
        CHECK(m.write_registers(0x3C, {0x00, 0xAE, 0xA8, 0x3F, 0xA1, 0xC8, 0x20, 0x00, 0x21, 0x00, 0x7F, 0x22, 0x00, 0x07,
                                          0x8D, 0x14, 0xAF}));
        std::vector<std::uint8_t> data(1025, 0);
        data[0] = 0x40;
        data[1] = 0x01;    // page 0, column 0, bit 0: the top-left pixel
        data[1024] = 0x80; // page 7, column 127, bit 7: the bottom-right pixel
        CHECK(m.write_registers(0x3C, data));
        const std::string text = d->text();
        REQUIRE(text.size() == 6 + 64 * 33);
        CHECK(text.substr(0, 6) == "128x64");
        CHECK(text.substr(7, 32) == "80000000000000000000000000000000");
        CHECK(text.substr(7 + 63 * 33, 32) == "00000000000000000000000000000001");
        CHECK(m.write_registers(0x3C, {0x80, 0xA7})); // Co set: one command, inverse
        CHECK(d->text().substr(7, 2) == "7F");
        CHECK(m.write_registers(0x3C, {0x00, 0xAE}));
        CHECK(d->text() == "128x64 off");
        CHECK(!m.write_registers(0x3D, {0x00, 0xAF})); // another address: no acknowledge
    }
}

TEST_CASE("devices: example 38 shows a DS3231 and a BMP280 on an SSD1306 OLED (SN-223)")
{
    REQUIRE(test::engine() != nullptr);
    const auto loaded = io::load_project(std::filesystem::path(SIMNODUS_TEST_EXAMPLES_DIR) / "38_I2C_OLED_BMP280_DS3231.snx");
    REQUIRE(loaded.schematic.has_value());
    sim::RunRequest request;
    request.schematic = *loaded.schematic;
    auto settings = request.schematic.settings();
    settings.speed = 0.0;
    request.schematic.set_settings(settings);
    request.library = &device_library();
    model::Id oled = model::no_id;
    for (const auto& c : request.schematic.components()) {
        auto changed = c;
        if (c.type == "ssd1306") {
            oled = c.id;
        } else if (c.type == "ds3231") {
            changed.parameters["start"] = "2026-10-30 07:59:59"; // the release day of 0.1
            request.schematic.put(changed);
        }
    }
    std::string error;
    std::vector<std::string> warnings;
    sim::McuEnvironment environment;
    environment.project_dir = SIMNODUS_TEST_EXAMPLES_DIR;
    REQUIRE(sim::build_mcu_requests(request.schematic, device_library(), environment, request.mcus, error, &warnings));
    CHECK(warnings.empty());
    REQUIRE(request.mcus.size() == 1);
    CHECK(request.mcus[0].options.devices.size() == 3);
    sim::SimulationSession session;
    REQUIRE(session.start(request, error));
    const auto begin = std::chrono::steady_clock::now();
    while (session.status().sim_time < 1.2) {
        REQUIRE(session.active());
        REQUIRE(std::chrono::steady_clock::now() - begin < 120s);
        std::this_thread::sleep_for(2ms);
    }
    const auto texts = session.device_texts();
    REQUIRE(texts.count(oled));
    const std::string& screen = texts.at(oled);
    REQUIRE(screen.rfind("128x64\n", 0) == 0);
    // Rows 0-6 hold the title in 5x7 glyphs; rows 42-48 "TEMP 24.5 C": something is lit there.
    std::vector<std::string> rows;
    std::size_t start = 7;
    while (start < screen.size()) {
        const auto end = screen.find('\n', start);
        rows.push_back(screen.substr(start, end == std::string::npos ? std::string::npos : end - start));
        start = end == std::string::npos ? screen.size() : end + 1;
    }
    REQUIRE(rows.size() == 64);
    auto lit = [&](int first, int last) {
        for (int r = first; r <= last; ++r) {
            if (rows[static_cast<std::size_t>(r)].find_first_not_of('0') != std::string::npos) {
                return true;
            }
        }
        return false;
    };
    CHECK(lit(0, 6));
    CHECK(lit(12, 25));
    CHECK(lit(42, 48));
    CHECK(lit(52, 58));
    CHECK(!lit(59, 63));
    // The values themselves, read with the firmware's 5x7 font (owner's test: the pressure was shown too high).
    static const std::string chars = " 0123456789:.-ACDEHILMNOPRSTU";
    static const std::uint8_t font[][5] = {{0x00, 0x00, 0x00, 0x00, 0x00}, {0x3E, 0x51, 0x49, 0x45, 0x3E},
        {0x00, 0x42, 0x7F, 0x40, 0x00}, {0x42, 0x61, 0x51, 0x49, 0x46}, {0x41, 0x49, 0x49, 0x49, 0x36},
        {0x18, 0x14, 0x12, 0x7F, 0x10}, {0x27, 0x45, 0x45, 0x45, 0x39}, {0x3E, 0x49, 0x49, 0x49, 0x30},
        {0x01, 0x71, 0x09, 0x05, 0x03}, {0x36, 0x49, 0x49, 0x49, 0x36}, {0x06, 0x49, 0x49, 0x49, 0x3E},
        {0x00, 0x00, 0x36, 0x00, 0x00}, {0x00, 0x60, 0x60, 0x00, 0x00}, {0x08, 0x08, 0x08, 0x08, 0x08},
        {0x7E, 0x09, 0x09, 0x09, 0x7E}, {0x3E, 0x41, 0x41, 0x41, 0x22}, {0x7F, 0x41, 0x41, 0x41, 0x3E},
        {0x7F, 0x49, 0x49, 0x49, 0x41}, {0x7F, 0x08, 0x08, 0x08, 0x7F}, {0x00, 0x41, 0x7F, 0x41, 0x00},
        {0x7F, 0x40, 0x40, 0x40, 0x40}, {0x7F, 0x02, 0x0C, 0x02, 0x7F}, {0x7F, 0x02, 0x04, 0x08, 0x7F},
        {0x3E, 0x41, 0x41, 0x41, 0x3E}, {0x7F, 0x09, 0x09, 0x09, 0x06}, {0x7F, 0x09, 0x19, 0x29, 0x46},
        {0x46, 0x49, 0x49, 0x49, 0x31}, {0x01, 0x01, 0x7F, 0x01, 0x01}, {0x3F, 0x40, 0x40, 0x40, 0x3F}};
    auto pixel = [&](int x, int y) {
        const auto& row = rows[static_cast<std::size_t>(y)];
        const int nibble = std::stoi(row.substr(static_cast<std::size_t>(x / 4), 1), nullptr, 16);
        return ((nibble >> (3 - x % 4)) & 1) != 0;
    };
    auto read = [&](int y) {
        std::string text;
        for (int x = 0; x + 5 <= 128; x += 6) {
            char found = '?';
            for (std::size_t g = 0; g < chars.size(); ++g) {
                bool same = true;
                for (int c = 0; c < 5 && same; ++c) {
                    for (int r = 0; r < 7 && same; ++r) {
                        same = pixel(x + c, y + r) == (((font[g][c] >> r) & 1) != 0);
                    }
                }
                if (same) {
                    found = chars[g];
                    break;
                }
            }
            text += found;
        }
        return text.substr(0, text.find_last_not_of(' ') + 1);
    };
    CHECK(read(42) == "TEMP 24.5 C");
    CHECK(read(52) == "PRES 1013.2 HPA");
    session.stop();
}

namespace {

// Drives a device's CS, SCK and MOSI like an SPI master in mode 0 and reads
// MISO just before each rising edge.
class SpiMaster {
public:
    SpiMaster(devices::Device& device, std::size_t cs, std::size_t sck, std::size_t mosi, std::size_t miso)
        : d_(device), cs_(cs), sck_(sck), mosi_(mosi), miso_(miso)
    {
        d_.initial_input(cs_, true);
        d_.initial_input(sck_, false);
        d_.initial_input(mosi_, false);
    }
    std::uint64_t& time() { return t_; }
    void select() { d_.set_input(cs_, false, t_ += 1000); }
    void release() { d_.set_input(cs_, true, t_ += 1000); }
    std::uint8_t byte(std::uint8_t out)
    {
        unsigned in = 0;
        for (int i = 7; i >= 0; --i) {
            d_.set_input(mosi_, ((out >> i) & 1) != 0, t_ += 250);
            const auto o = d_.output(miso_);
            in = (in << 1) | (o.driven && o.high ? 1u : 0u);
            d_.set_input(sck_, true, t_ += 250);
            d_.set_input(sck_, false, t_ += 500);
        }
        return static_cast<std::uint8_t>(in);
    }
    std::vector<std::uint8_t> transfer(const std::vector<std::uint8_t>& bytes)
    {
        select();
        std::vector<std::uint8_t> in;
        for (const auto b : bytes) {
            in.push_back(byte(b));
        }
        release();
        return in;
    }

private:
    devices::Device& d_;
    std::size_t cs_, sck_, mosi_, miso_;
    std::uint64_t t_ = 0;
};

} // namespace

TEST_CASE("devices: SPI converters, potentiometer, flash, card and the HX711 follow their datasheets (SN-224)")
{
    std::string error;
    { // MCP3208: 1.25 V on CH2 against a 2.5 V reference reads 2048; a tied input reads its rail.
        const std::vector<std::string> pins{"CH0", "CH1", "CH2", "CH3", "CH4", "CH5", "CH6", "CH7", "CS", "CLK", "DIN",
            "DOUT", "VREF", "AGND"};
        auto d = devices::make_device("mcp3208", pins, {{"@supply", "3.3"}}, error);
        REQUIRE(d);
        d->set_analog_source([](std::size_t pin, std::uint64_t) {
            return pin == 2 ? 1.25 : pin == 12 ? 2.5 : pin == 3 ? 3.0 : std::nan("");
        });
        d->initial_input(7, true); // CH7 tied to the supply
        SpiMaster m(*d, 8, 9, 10, 11);
        auto read = [&](unsigned channel, bool single) {
            const auto r = m.transfer({static_cast<std::uint8_t>(0x04 | (single ? 2 : 0) | (channel >> 2)),
                static_cast<std::uint8_t>((channel & 3) << 6), 0x00});
            return ((r[1] & 0x0F) << 8) | r[2];
        };
        CHECK(read(2, true) == 2048);
        CHECK(read(7, true) == 4095); // 3.3 V over 2.5 V clamps
        CHECK(read(0, true) == 0);
        CHECK(read(2, false) == 0);    // CH2+ CH3-: 1.25 - 3.0 V is negative
        CHECK(read(3, false) == 2867); // CH2- CH3+: 1.75 V
    }
    { // MCP41010: write 0x40 (a quarter of the track); shutdown; the wiper as the strength of WIPER.
        auto d = devices::make_device("mcp41010", {"CS", "SCK", "SI", "WIPER"}, {}, error);
        REQUIRE(d);
        SpiMaster m(*d, 0, 1, 2, 3);
        CHECK(std::abs(d->output(3).strength - 0.5) < 1e-9);
        m.transfer({0x11, 0x40});
        CHECK(std::abs(d->output(3).strength - 0.25) < 1e-9);
        m.transfer({0x11}); // only 8 bits: ignored
        CHECK(std::abs(d->output(3).strength - 0.25) < 1e-9);
        m.transfer({0x21, 0x00});
        CHECK(std::abs(d->output(3).strength - 0.0) < 1e-9);
    }
    { // W25Q32: IDs, write enable, page program with wrap, busy, read back, sector erase.
        auto d = devices::make_device("w25q32", {"CS", "CLK", "DI", "DO", "WP", "HOLD"}, {}, error);
        REQUIRE(d);
        SpiMaster m(*d, 0, 1, 2, 3);
        CHECK(m.transfer({0x9F, 0, 0, 0}) == (std::vector<std::uint8_t>{0xFF, 0xEF, 0x40, 0x16}));
        CHECK(m.transfer({0x90, 0, 0, 0, 0, 0})[4] == 0xEF);
        m.transfer({0x02, 0x00, 0x10, 0xFE, 0x12}); // no WEL: ignored
        CHECK(m.transfer({0x03, 0x00, 0x10, 0xFE, 0})[4] == 0xFF);
        m.transfer({0x06});
        CHECK((m.transfer({0x05, 0})[1] & 2) == 2); // WEL
        m.transfer({0x02, 0x00, 0x10, 0xFE, 0x12, 0x34, 0x56}); // 0x56 wraps to the start of the page
        CHECK((m.transfer({0x05, 0})[1] & 1) == 1);              // BUSY
        m.transfer({0x06});                                      // ignored while busy
        m.time() += 1000000;
        d->advance(m.time());
        CHECK(m.transfer({0x05, 0})[1] == 0);
        CHECK(m.transfer({0x03, 0x00, 0x10, 0xFE, 0, 0}) == (std::vector<std::uint8_t>{0xFF, 0xFF, 0xFF, 0xFF, 0x12, 0x34}));
        CHECK(m.transfer({0x0B, 0x00, 0x10, 0x00, 0, 0})[5] == 0x56); // fast read: a dummy byte
        CHECK(d->memory()[0x10FE] == 0x12);
        m.transfer({0x06});
        m.transfer({0x20, 0x00, 0x10, 0x00});
        m.time() += 50000000;
        d->advance(m.time());
        CHECK(m.transfer({0x03, 0x00, 0x10, 0xFE, 0})[4] == 0xFF);
    }
    { // microSD: initialization, a block written and read back, an illegal command.
        auto d = devices::make_device("sdcard", {"CS", "SCK", "MOSI", "MISO"}, {{"size", "1M"}}, error);
        REQUIRE(d);
        SpiMaster m(*d, 0, 1, 2, 3);
        auto command = [&](std::uint8_t index, std::uint32_t argument, std::size_t answer) {
            m.select();
            for (const auto b : {static_cast<std::uint8_t>(0x40 | index), static_cast<std::uint8_t>(argument >> 24),
                     static_cast<std::uint8_t>(argument >> 16), static_cast<std::uint8_t>(argument >> 8),
                     static_cast<std::uint8_t>(argument), std::uint8_t{0x95}}) {
                m.byte(b);
            }
            std::vector<std::uint8_t> r;
            std::uint8_t b = 0xFF;
            for (int i = 0; i < 8 && b == 0xFF; ++i) {
                b = m.byte(0xFF);
            }
            r.push_back(b);
            for (std::size_t i = 1; i < answer; ++i) {
                r.push_back(m.byte(0xFF));
            }
            return r;
        };
        CHECK(command(0, 0, 1)[0] == 0x01);
        m.release();
        CHECK(command(8, 0x1AA, 5) == (std::vector<std::uint8_t>{0x01, 0x00, 0x00, 0x01, 0xAA}));
        m.release();
        CHECK(command(55, 0, 1)[0] == 0x01);
        m.release();
        CHECK(command(41, 0x40000000, 1)[0] == 0x00);
        m.release();
        CHECK(command(58, 0, 5)[1] == 0xC0);
        m.release();
        // Write block 3.
        CHECK(command(24, 3, 1)[0] == 0x00);
        m.byte(0xFF);
        m.byte(0xFE);
        for (int i = 0; i < 512; ++i) {
            m.byte(static_cast<std::uint8_t>(i));
        }
        m.byte(0xFF);
        m.byte(0xFF);
        CHECK((m.byte(0xFF) & 0x1F) == 0x05);
        while (m.byte(0xFF) != 0xFF) {
        }
        m.release();
        CHECK(d->memory()[3 * 512 + 7] == 7);
        // Read it back.
        CHECK(command(17, 3, 1)[0] == 0x00);
        std::uint8_t token = 0xFF;
        for (int i = 0; i < 16 && token == 0xFF; ++i) {
            token = m.byte(0xFF);
        }
        CHECK(token == 0xFE);
        std::vector<std::uint8_t> block;
        for (int i = 0; i < 512; ++i) {
            block.push_back(m.byte(0xFF));
        }
        m.release();
        CHECK(block[200] == 200);
        CHECK((command(5, 0, 1)[0] & 0x04) == 0x04); // CMD5: illegal in SPI mode
        m.release();
    }
    { // HX711: 1 mV on channel A at 5 V excitation and gain 128 is 1e-3 / (2.5 / 128) x 2^23.
        auto d = devices::make_device("hx711", {"SCK", "DT", "E+", "E-", "A+", "A-", "B+", "B-"}, {{"@supply", "5"}},
            error);
        REQUIRE(d);
        d->set_analog_source([](std::size_t pin, std::uint64_t) {
            return pin == 2 ? 5.0 : pin == 4 ? 2.5005 : pin == 5 ? 2.4995 : std::nan("");
        });
        auto dt = [&] {
            const auto o = d->output(1);
            return o.driven && o.high;
        };
        CHECK(dt()); // converting
        d->advance(d->next_event_ns());
        CHECK(!dt()); // ready
        std::uint64_t t = d->next_event_ns() - 50000000;
        std::uint32_t value = 0;
        for (int i = 0; i < 25; ++i) {
            d->set_input(0, true, t += 1000);
            if (i < 24) {
                value = (value << 1) | (dt() ? 1u : 0u);
            }
            d->set_input(0, false, t += 1000);
        }
        CHECK(value == 429497); // 0.001 / 0.01953125 x 8388608, rounded
        CHECK(dt());             // read: high until the next conversion
    }
}

TEST_CASE("devices: example 39 reads voltages, sets a digital potentiometer, writes a flash and weighs (SN-224)")
{
    REQUIRE(test::engine() != nullptr);
    const auto loaded =
        io::load_project(std::filesystem::path(SIMNODUS_TEST_EXAMPLES_DIR) / "39_SPI_ADC_digital_pot_flash_HX711.snx");
    REQUIRE(loaded.schematic.has_value());
    sim::RunRequest request;
    request.schematic = *loaded.schematic;
    auto settings = request.schematic.settings();
    settings.speed = 0.0;
    request.schematic.set_settings(settings);
    request.library = &device_library();
    request.vectors = {"pot", "dpot"};
    model::Id board = model::no_id;
    for (const auto& c : request.schematic.components()) {
        board = c.type == "bluepill" ? c.id : board;
    }
    std::string error;
    std::vector<std::string> warnings;
    sim::McuEnvironment environment;
    environment.project_dir = SIMNODUS_TEST_EXAMPLES_DIR;
    REQUIRE(sim::build_mcu_requests(request.schematic, device_library(), environment, request.mcus, error, &warnings));
    for (const auto& w : warnings) {
        std::printf("  warning: %s\n", w.c_str());
    }
    CHECK(warnings.empty());
    REQUIRE(request.mcus.size() == 1);
    CHECK(request.mcus[0].options.devices.size() == 4);
    sim::SimulationSession session;
    REQUIRE(session.start(request, error));
    const auto begin = std::chrono::steady_clock::now();
    while (session.status().sim_time < 1.2) {
        REQUIRE(session.active());
        REQUIRE(std::chrono::steady_clock::now() - begin < 120s);
        std::this_thread::sleep_for(2ms);
    }
    std::string text;
    session.mcu_uart_received(board, "usart1", 0, text);
    std::printf("%s", text.c_str());
    CHECK(text.find("W25Q32 ID EF4016") != std::string::npos);
    CHECK(text.find("W25Q32 reads SimNodus") != std::string::npos);
    // RV1 at 30 %: 2.31 V of 3.3 V is code 2867; the digital potentiometer follows within a step (16 codes).
    CHECK(text.find("CH0 2867 CH1 28") != std::string::npos);
    CHECK(text.find("LOAD 19") != std::string::npos); // 40 % of 5 kg, about 2000 g
    CHECK(std::abs(session.latest("dpot").value_or(0) - session.latest("pot").value_or(0)) < 0.03);
    session.stop();
}

namespace {

// One wire between a master and a device with an open-drain or push-pull
// output on the same pin; the device's timed events run as time passes.
class Wire {
public:
    Wire(devices::Device& device, std::size_t pin) : d_(device), pin_(pin) { d_.initial_input(pin_, true); }
    std::uint64_t now() const { return t_; }
    bool level() const { return net_; }
    void run_until(std::uint64_t until)
    {
        while (d_.next_event_ns() <= until) {
            t_ = std::max(t_, d_.next_event_ns());
            d_.advance(t_);
            settle();
        }
        t_ = std::max(t_, until);
    }
    void drive(bool level, std::uint64_t after_ns)
    {
        run_until(t_ + after_ns);
        master_ = level;
        settle();
    }
    // Times (ns) at which the wire changed, while running for `span_ns`.
    std::vector<std::pair<std::uint64_t, bool>> record(std::uint64_t span_ns)
    {
        std::vector<std::pair<std::uint64_t, bool>> edges;
        const std::uint64_t end = t_ + span_ns;
        bool last = net_;
        while (d_.next_event_ns() <= end) {
            t_ = std::max(t_, d_.next_event_ns());
            d_.advance(t_);
            settle();
            if (net_ != last) {
                edges.emplace_back(t_, net_);
                last = net_;
            }
        }
        t_ = end;
        return edges;
    }

private:
    void settle()
    {
        const auto o = d_.output(pin_);
        const bool net = master_ && !(o.driven && !o.high);
        if (net != net_) {
            net_ = net;
            d_.set_input(pin_, net, t_);
            settle();
        }
    }
    devices::Device& d_;
    std::size_t pin_;
    std::uint64_t t_ = 0;
    bool master_ = true, net_ = true;
};

// 1-Wire master slots (Maxim application note 126 timings).
bool one_wire_reset(Wire& w)
{
    w.drive(false, 1000);
    w.drive(true, 500000);
    w.run_until(w.now() + 70000);
    const bool presence = !w.level();
    w.run_until(w.now() + 430000);
    return presence;
}
void one_wire_write(Wire& w, std::uint8_t byte)
{
    for (int k = 0; k < 8; ++k) {
        const bool one = ((byte >> k) & 1) != 0;
        w.drive(false, 2000);
        w.drive(true, one ? 6000 : 60000);
        w.run_until(w.now() + (one ? 64000 : 10000));
    }
}
std::uint8_t one_wire_read(Wire& w)
{
    unsigned value = 0;
    for (int k = 0; k < 8; ++k) {
        w.drive(false, 2000);
        w.drive(true, 6000);
        w.run_until(w.now() + 9000);
        value |= (w.level() ? 1u : 0u) << k;
        w.run_until(w.now() + 55000);
    }
    return static_cast<std::uint8_t>(value);
}

std::uint8_t maxim_crc8(const std::vector<std::uint8_t>& bytes)
{
    std::uint8_t crc = 0;
    for (auto b : bytes) {
        for (int k = 0; k < 8; ++k) {
            const bool mix = ((crc ^ b) & 1) != 0;
            crc = static_cast<std::uint8_t>(crc >> 1);
            if (mix) {
                crc ^= 0x8C;
            }
            b = static_cast<std::uint8_t>(b >> 1);
        }
    }
    return crc;
}

} // namespace

TEST_CASE("devices: DS18B20, DHT22, HC-SR04 and WS2812B keep their protocols' timing (SN-225)")
{
    std::string error;
    { // DS18B20: presence, READ ROM with a valid CRC, +85 C before converting, 23.5 C after 750 ms.
        auto d = devices::make_device("ds18b20", {"DQ"}, {{"temperature", "23.5"}, {"@designator", "U7"}}, error);
        REQUIRE(d);
        Wire w(*d, 0);
        REQUIRE(one_wire_reset(w));
        one_wire_write(w, 0x33);
        std::vector<std::uint8_t> rom;
        for (int i = 0; i < 8; ++i) {
            rom.push_back(one_wire_read(w));
        }
        CHECK(rom[0] == 0x28);
        CHECK(maxim_crc8(rom) == 0); // the CRC over all eight bytes
        auto scratchpad = [&] {
            REQUIRE(one_wire_reset(w));
            one_wire_write(w, 0xCC);
            one_wire_write(w, 0xBE);
            std::vector<std::uint8_t> s;
            for (int i = 0; i < 9; ++i) {
                s.push_back(one_wire_read(w));
            }
            return s;
        };
        auto s = scratchpad();
        CHECK(maxim_crc8(s) == 0);
        CHECK((s[0] | (s[1] << 8)) == 85 * 16);
        REQUIRE(one_wire_reset(w));
        one_wire_write(w, 0xCC);
        one_wire_write(w, 0x44);
        CHECK(one_wire_read(w) == 0x00); // converting
        w.run_until(w.now() + 760000000);
        CHECK(one_wire_read(w) == 0xFF); // done
        s = scratchpad();
        CHECK((s[0] | (s[1] << 8)) == 376); // 23.5 x 16
        // Search ROM with one sensor finds its code: bit, complement, direction.
        REQUIRE(one_wire_reset(w));
        one_wire_write(w, 0xF0);
        std::vector<std::uint8_t> found(8, 0);
        for (int bit = 0; bit < 64; ++bit) {
            auto slot = [&] {
                w.drive(false, 2000);
                w.drive(true, 6000);
                w.run_until(w.now() + 9000);
                const bool level = w.level();
                w.run_until(w.now() + 55000);
                return level;
            };
            const bool b = slot();
            const bool complement = slot();
            CHECK(b != complement);
            found[static_cast<std::size_t>(bit / 8)] |= static_cast<std::uint8_t>((b ? 1u : 0u) << (bit % 8));
            w.drive(false, 2000);
            w.drive(true, b ? 6000 : 60000);
            w.run_until(w.now() + (b ? 64000 : 10000));
        }
        CHECK(found == rom);
    }
    { // DHT22: 40 bits after the start signal: 65.3 %RH, -4.2 C.
        auto d = devices::make_device("dht22", {"DATA"}, {{"temperature", "-4.2"}, {"humidity", "65.3"}}, error);
        REQUIRE(d);
        Wire w(*d, 0);
        w.drive(false, 1000);
        w.drive(true, 1100000);
        const auto edges = w.record(10000000);
        // Highs after the 80 us response: 40 data bits, by their length.
        std::vector<std::uint64_t> highs;
        for (std::size_t i = 1; i < edges.size(); ++i) {
            if (!edges[i].second && edges[i - 1].second) {
                highs.push_back(edges[i].first - edges[i - 1].first);
            }
        }
        REQUIRE(highs.size() == 41);
        CHECK(highs[0] == 80000);
        std::array<std::uint8_t, 5> b{};
        for (int i = 0; i < 40; ++i) {
            b[static_cast<std::size_t>(i / 8)] =
                static_cast<std::uint8_t>((b[static_cast<std::size_t>(i / 8)] << 1) | (highs[static_cast<std::size_t>(i + 1)] > 50000 ? 1 : 0));
        }
        CHECK(((b[0] << 8) | b[1]) == 653);
        CHECK(((b[2] << 8) | b[3]) == (0x8000 | 42));
        CHECK(b[4] == static_cast<std::uint8_t>(b[0] + b[1] + b[2] + b[3]));
        CHECK(w.record(5000000).empty()); // one answer per start signal
    }
    { // HC-SR04: 100 cm is an echo of 5.83 ms.
        auto d = devices::make_device("hcsr04", {"TRIG", "ECHO"}, {{"distance", "100"}}, error);
        REQUIRE(d);
        d->initial_input(0, false);
        d->set_input(0, true, 1000000);
        d->set_input(0, false, 1012000);
        CHECK(!d->output(1).high);
        d->advance(d->next_event_ns());
        CHECK(d->output(1).high);
        const std::uint64_t rose = d->next_event_ns();
        d->advance(rose);
        CHECK(!d->output(1).high);
        (void)rose;
    }
    { // WS2812B: red then blue on the first two LEDs, shown after the reset time.
        auto d = devices::make_device("ws2812", {"DIN", "DOUT"}, {{"count", "8"}}, error);
        REQUIRE(d);
        d->initial_input(0, false);
        std::uint64_t t = 10000;
        auto send = [&](std::uint32_t grb) {
            for (int k = 23; k >= 0; --k) {
                const bool one = ((grb >> k) & 1u) != 0;
                d->set_input(0, true, t);
                d->set_input(0, false, t + (one ? 800 : 400));
                t += 1250;
            }
        };
        send(0x00FF00); // GRB: red
        send(0x0000FF); // blue
        CHECK(d->text().find("000000000000") != std::string::npos); // not shown yet
        d->advance(d->next_event_ns());
        CHECK(d->text() == "8x1 rgb\nFF00000000FF000000000000000000000000000000000000");
    }
}

TEST_CASE("devices: example 40 reads a DS18B20, a DHT22 and an HC-SR04 and lights a WS2812B strip (SN-225)")
{
    REQUIRE(test::engine() != nullptr);
    const auto loaded =
        io::load_project(std::filesystem::path(SIMNODUS_TEST_EXAMPLES_DIR) / "40_OneWire_DS18B20_DHT22_HCSR04_WS2812.snx");
    REQUIRE(loaded.schematic.has_value());
    sim::RunRequest request;
    request.schematic = *loaded.schematic;
    auto settings = request.schematic.settings();
    settings.speed = 0.0;
    request.schematic.set_settings(settings);
    request.library = &device_library();
    model::Id board = model::no_id, strip = model::no_id, sensor = model::no_id;
    for (const auto& c : request.schematic.components()) {
        board = c.type == "bluepill" ? c.id : board;
        strip = c.type == "ws2812" ? c.id : strip;
        sensor = c.type == "ds18b20" ? c.id : sensor;
    }
    std::string error;
    std::vector<std::string> warnings;
    sim::McuEnvironment environment;
    environment.project_dir = SIMNODUS_TEST_EXAMPLES_DIR;
    REQUIRE(sim::build_mcu_requests(request.schematic, device_library(), environment, request.mcus, error, &warnings));
    CHECK(warnings.empty());
    sim::SimulationSession session;
    REQUIRE(session.start(request, error));
    const auto begin = std::chrono::steady_clock::now();
    auto wait_for = [&](double t) {
        while (session.status().sim_time < t) {
            REQUIRE(session.active());
            REQUIRE(std::chrono::steady_clock::now() - begin < 120s);
            std::this_thread::sleep_for(2ms);
        }
    };
    wait_for(1.5);
    // The presence pulse comes 30 us after the reset ends (the device's event, not the next window).
    const auto log = session.mcu_pin_log(board, "PB5", 0);
    REQUIRE(log.changes.size() > 4);
    const auto presence = log.changes[2].first - log.changes[1].first;
    CHECK(presence >= 29900);
    CHECK(presence <= 30200); // within the core's cycles of the event
    session.set_device_parameter(sensor, "temperature", "-10.5");
    wait_for(3.4);
    std::string text;
    session.mcu_uart_received(board, "usart1", 0, text);
    std::printf("%s", text.c_str());
    CHECK(text.find("DS18B20 22.5 C  DHT22 24.0 C 61.5 %  HC-SR04 120 cm") != std::string::npos);
    CHECK(text.find("DS18B20 -10.5 C") != std::string::npos);
    const auto texts = session.device_texts();
    REQUIRE(texts.count(strip));
    // 120 cm: five LEDs of the distance bar, green to red; three off.
    CHECK(texts.at(strip).substr(texts.at(strip).size() - 18) == "000000000000000000");
    CHECK(texts.at(strip).substr(8, 6) == "00FF00");
    session.stop();
}

TEST_CASE("devices: a hobby servo follows the pulse width at its speed (SN-226)")
{
    std::string error;
    auto d = devices::make_device("servo", {"SIG"}, {{"min", "500"}, {"max", "2500"}}, error);
    REQUIRE(d);
    CHECK(d->text() == "dial 90.0");
    std::uint64_t t = 0;
    auto pulse = [&](std::uint64_t width_us) {
        d->set_input(0, true, t);
        d->set_input(0, false, t + width_us * 1000);
        t += 20000000; // 50 Hz
        while (d->next_event_ns() <= t) {
            d->advance(d->next_event_ns());
        }
    };
    pulse(2500);
    CHECK(d->text() != "dial 90.0"); // on its way: 600 degrees a second
    for (int i = 0; i < 10; ++i) {
        pulse(2500);
    }
    CHECK(d->text() == "dial 180.0");
    for (int i = 0; i < 20; ++i) {
        pulse(1000);
    }
    CHECK(d->text() == "dial 45.0");
    pulse(5000); // not a servo pulse: ignored
    CHECK(d->text() == "dial 45.0");
}

TEST_CASE("devices: example 41 turns a servo, a stepper and a DC motor with an encoder (SN-226)")
{
    REQUIRE(test::engine() != nullptr);
    const auto loaded =
        io::load_project(std::filesystem::path(SIMNODUS_TEST_EXAMPLES_DIR) / "41_Motion_encoder_servo_stepper_L298.snx");
    REQUIRE(loaded.schematic.has_value());
    sim::RunRequest request;
    request.schematic = *loaded.schematic;
    auto settings = request.schematic.settings();
    settings.speed = 0.0;
    request.schematic.set_settings(settings);
    request.library = &device_library();
    request.vectors = {"ma", "mb"};
    model::Id board = model::no_id, encoder = model::no_id, servo = model::no_id;
    for (const auto& c : request.schematic.components()) {
        board = c.type == "bluepill" ? c.id : board;
        encoder = c.type == "encoder" ? c.id : encoder;
        servo = c.type == "servo" ? c.id : servo;
    }
    std::string error;
    std::vector<std::string> warnings;
    sim::McuEnvironment environment;
    environment.project_dir = SIMNODUS_TEST_EXAMPLES_DIR;
    REQUIRE(sim::build_mcu_requests(request.schematic, device_library(), environment, request.mcus, error, &warnings));
    CHECK(warnings.empty());
    sim::SimulationSession session;
    REQUIRE(session.start(request, error));
    const auto begin = std::chrono::steady_clock::now();
    auto wait_for = [&](double t) {
        while (session.status().sim_time < t) {
            REQUIRE(session.active());
            REQUIRE(std::chrono::steady_clock::now() - begin < 120s);
            std::this_thread::sleep_for(2ms);
        }
    };
    // Two detents clockwise, a quarter at a time, as a hand turns it (slowly enough that the
    // run, ahead of this thread by a few milliseconds, never applies two quarters at once).
    double t = 0.3;
    for (int k = 1; k <= 8; ++k) {
        wait_for(t);
        session.set_control(encoder, (10 + k * 0.25) / 20);
        t += 0.1;
    }
    wait_for(2.4);
    std::string text;
    session.mcu_uart_received(board, "usart1", 0, text);
    std::printf("%s", text.c_str());
    CHECK(text.find("POS 12 SERVO 108") != std::string::npos);
    CHECK(session.device_texts().at(servo) == "dial 108.0");
    CHECK(session.latest("ma").value_or(0) - session.latest("mb").value_or(0) > -1.0); // forward or freewheeling
    session.stop();
}

TEST_CASE("devices: example 42 plays a scale the passive buzzer reports and beeps the active one (SN-228)")
{
    REQUIRE(test::engine() != nullptr);
    const auto loaded = io::load_project(std::filesystem::path(SIMNODUS_TEST_EXAMPLES_DIR) / "42_Buzzers_melody_and_beep.snx");
    REQUIRE(loaded.schematic.has_value());
    sim::RunRequest request;
    request.schematic = *loaded.schematic;
    auto settings = request.schematic.settings();
    settings.speed = 0.0;
    request.schematic.set_settings(settings);
    request.library = &device_library();
    request.vectors = {"q_bz1", "q_bz2"};
    std::string error;
    std::vector<std::string> warnings;
    sim::McuEnvironment environment;
    environment.project_dir = SIMNODUS_TEST_EXAMPLES_DIR;
    REQUIRE(sim::build_mcu_requests(request.schematic, device_library(), environment, request.mcus, error, &warnings));
    CHECK(warnings.empty());
    sim::SimulationSession session;
    REQUIRE(session.start(request, error));
    const auto begin = std::chrono::steady_clock::now();
    auto wait_for = [&](double t) {
        while (session.status().sim_time < t) {
            REQUIRE(session.active());
            REQUIRE(std::chrono::steady_clock::now() - begin < 120s);
            std::this_thread::sleep_for(2ms);
        }
    };
    // The third note (E5, 659 Hz) sounds from 0.6 to 0.9 s; the 50 ms average has settled by 0.85 s.
    wait_for(0.85);
    std::printf("  passive buzzer at 0.85 s: %.0f Hz\n", session.latest("q_bz1").value_or(0));
    CHECK(std::abs(session.latest("q_bz1").value_or(0) - 659) < 25);
    CHECK(session.latest("q_bz2").value_or(1) < 1); // the active buzzer waits for the end of the scale
    // 15 notes of 0.3 s, then the active buzzer for 0.2 s.
    wait_for(4.6);
    CHECK(std::abs(session.latest("q_bz2").value_or(0) - 2300) < 1);
    session.stop();
}
