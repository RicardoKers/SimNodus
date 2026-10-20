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

#include <chrono>
#include <cstring>
#include <filesystem>
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
        {"DS", {"PA7"}, {{0, "DS"}}, 0},
        {"SHCP", {"PA5"}, {{0, "SHCP"}}, 0},
        {"STCP", {"PA4"}, {{0, "STCP"}}, 0},
        {"BACK", {"PA0"}, {{0, "Q7S"}}, 0},
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
    REQUIRE(requests.size() == 1);
    const auto& options = requests[0].options;
    REQUIRE(options.devices.size() == 2); // the one on X has no board
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
    CHECK(has("not connected to a microcontroller pin"));
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
    const std::uint64_t lit_until = d->next_event_ns();
    CHECK(lit_until - t == slot * 15 / 32);
    d->advance(lit_until);
    CHECK(scan_state(*d).second == 0); // the dark part of the slot
    d->advance(d->next_event_ns());
    state = scan_state(*d);
    CHECK(state.first == 1);
    CHECK(state.second == 0xC0); // G and DP
    d->advance(d->next_event_ns());
    d->advance(d->next_event_ns());
    CHECK(scan_state(*d).first == 0); // back to digit 0 (scan limit 1)
    word(0x0F01);                     // display test: every segment of eight digits
    CHECK(scan_state(*d).second == 0xFF);
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
        {"DIN", {"PA7"}, {{0, "DIN"}}, 0},
        {"CLK", {"PA5"}, {{0, "CLK"}}, 0},
        {"LOAD", {"PA4"}, {{0, "LOAD"}}, 0},
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
}
