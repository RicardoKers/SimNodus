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
    for (const auto& c : request.schematic.components()) {
        board = c.type == "bluepill" ? c.id : board;
        button = c.designator == "SW1" ? c.id : button;
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
    const auto& options = request.mcus[0].options;
    CHECK(options.devices.size() == 3);
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
