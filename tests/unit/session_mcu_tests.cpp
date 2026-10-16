// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// A Blue Pill in an interactive run (SN-131): ngspice and Renode together.
// Skipped (and reported) when the build has no Renode.
#include "test_support.hpp"

#include "circuit_builder.hpp"
#include "engine_fixture.hpp"
#include "io/project_file.hpp"
#include "sim/process_memory.hpp"
#include "sim/simulation_session.hpp"

#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <set>
#include <thread>

using namespace simnodus;
using namespace std::chrono_literals;

extern std::string test_mcu_engine; // engine_tests.cpp ("" = the board's default: the own emulator)
extern double test_soak_segment;

// True unless the cases run on Renode (the legacy engine, D1 on 2026-10-16).
bool on_native()
{
    return test_mcu_engine != "renode";
}

namespace {

library::ComponentLibrary& board_library()
{
    static library::ComponentLibrary library = [] {
        library::ComponentLibrary l;
        l.load_directory(SIMNODUS_TEST_LIBRARY_DIR);
        return l;
    }();
    return library;
}

#ifdef SIMNODUS_TEST_RENODE
sim::McuEnvironment environment()
{
    sim::McuEnvironment e;
    e.engine = test_mcu_engine;
    e.project_dir = SIMNODUS_TEST_EXAMPLES_DIR;
    e.renode = std::filesystem::path(SIMNODUS_TEST_RENODE);
    e.resources = std::filesystem::path(SIMNODUS_TEST_MCU_DIR);
    return e;
}
#endif

sim::McuEnvironment folder_only(const std::filesystem::path& folder)
{
    sim::McuEnvironment e;
    e.project_dir = folder;
    return e;
}

} // namespace

TEST_CASE("session: boards without firmware or engine are explained")
{
    test::CircuitBuilder b(board_library());
    b.add("bluepill", {{"G1", "0"}});
    std::vector<sim::McuRequest> requests;
    std::string error;
    CHECK(!sim::build_mcu_requests(b.schematic, board_library(), folder_only("."), requests, error));
    CHECK(error.find("no firmware") != std::string::npos);

    test::CircuitBuilder missing(board_library());
    missing.add("bluepill", {{"G1", "0"}}, {{"firmware", "does/not/exist.elf"}});
    error.clear();
    CHECK(!sim::build_mcu_requests(missing.schematic, board_library(), folder_only("."), requests, error));
    CHECK(error.find("does not exist") != std::string::npos);

    // The own emulator (the default, ADR 0090) needs nothing installed; Renode, the
    // legacy engine, must be found when it is chosen.
    test::CircuitBuilder no_renode(board_library());
    no_renode.add("bluepill", {{"G1", "0"}}, {{"firmware", "firmware/blink/blink.elf"}});
    error.clear();
    requests.clear();
    CHECK(sim::build_mcu_requests(no_renode.schematic, board_library(), folder_only(SIMNODUS_TEST_EXAMPLES_DIR), requests, error));
    CHECK(requests.size() == 1 && requests.front().engine == "native");
    auto legacy = folder_only(SIMNODUS_TEST_EXAMPLES_DIR);
    legacy.engine = "renode";
    error.clear();
    CHECK(!sim::build_mcu_requests(no_renode.schematic, board_library(), legacy, requests, error));
    CHECK(error.find("Renode") != std::string::npos);
}

TEST_CASE("session: a Blue Pill runs its firmware alongside the circuit")
{
#ifndef SIMNODUS_TEST_RENODE
    std::printf("  skipped: no Renode in this build\n");
#else
    REQUIRE(test::engine() != nullptr);
    test::CircuitBuilder b(board_library());
    const model::Id board = b.add("bluepill", {{"G1", "0"}}, {{"firmware", "firmware/blink/blink.elf"}});
    b.add("resistor", {{"1", "N1"}, {"2", "0"}}, {{"R", "1k"}});
    b.add("vdc", {{"+", "N1"}, {"-", "0"}}, {{"V", "1"}});
    auto settings = b.schematic.settings();
    settings.speed = 0.0; // as fast as the slower engine allows
    settings.segment = 0.5;
    b.schematic.set_settings(settings);

    sim::RunRequest request;
    request.schematic = b.schematic;
    request.library = &board_library();
    request.vectors = {"N1"};
    std::string error;
    REQUIRE(sim::build_mcu_requests(b.schematic, board_library(), environment(), request.mcus, error));
    REQUIRE(request.mcus.size() == 1);
    CHECK(request.mcus.front().options.pins.size() == 32);

    sim::SimulationSession session;
    REQUIRE(session.start(request, error));
    const auto begin = std::chrono::steady_clock::now();
    bool saw_starting = false;
    while (session.status().starting_mcu || session.status().sim_time == 0.0) {
        saw_starting = saw_starting || session.status().starting_mcu;
        REQUIRE(session.active());
        REQUIRE(std::chrono::steady_clock::now() - begin < 180s);
        std::this_thread::sleep_for(10ms);
    }
    std::printf("  circuit started after %.1f s\n",
        std::chrono::duration<double>(std::chrono::steady_clock::now() - begin).count());
    if (!on_native()) {
        CHECK(saw_starting); // Renode takes seconds; the own emulator is ready at once
    }

    // Watch PC13 over 1.6 s of virtual time: the firmware toggles it every 500 ms.
    std::set<bool> levels;
    std::uint64_t worst_lead = 0;
    std::uint64_t worst_lag = 0;
    while (session.status().sim_time < 1.6) {
        REQUIRE(session.active());
        REQUIRE(std::chrono::steady_clock::now() - begin < 300s);
        const auto status = session.mcu_status(board);
        REQUIRE(status.has_value());
        REQUIRE(status->state == mcu::McuState::Running);
        const double analog = session.status().sim_time;
        const auto analog_us = static_cast<std::uint64_t>(analog * 1e6);
        const std::uint64_t firmware_us = status->time_ns / 1000;
        if (firmware_us > analog_us) {
            worst_lead = std::max(worst_lead, firmware_us - analog_us);
        } else {
            worst_lag = std::max(worst_lag, analog_us - firmware_us);
        }
        const auto pins = session.mcu_pins(board);
        if (firmware_us > 60000) { // modes are at most 50 ms older than the time
            levels.insert(pins.at("PC13").level);
            CHECK(pins.at("PC13").mode == mcu::PinMode::Output);
        }
        std::this_thread::sleep_for(20ms);
    }
    std::printf("  firmware ahead of the circuit by at most %llu us, behind by at most %llu us\n",
        static_cast<unsigned long long>(worst_lead), static_cast<unsigned long long>(worst_lag));
    CHECK(levels.size() == 2);
    // The limit also follows ngspice's tentative time point, up to one maximum step
    // (settings.max_step, 100 us by default) beyond the last accepted one.
    const auto lead = static_cast<std::uint64_t>(std::llround(b.schematic.settings().mcu_quantum * 1e6));
    CHECK(worst_lead <= 2 * lead + 2 * 100); // one chunk past the limit (held events, ADR 0085)
    // The status is sampled between the circuit's and the firmware's updates; allow one run chunk.
    CHECK(worst_lag <= lead + 10000);
    CHECK(session.latest("n1").has_value());
    session.stop();
    CHECK(session.mcu_status(board)->state == mcu::McuState::Stopped);
#endif
}

TEST_CASE("session: transient mode refuses microcontrollers")
{
    test::CircuitBuilder b(board_library());
    b.add("bluepill", {{"G1", "0"}}, {{"firmware", "firmware/blink/blink.elf"}});
    auto settings = b.schematic.settings();
    settings.mode = model::SimulationMode::Transient;
    b.schematic.set_settings(settings);
    sim::RunRequest request;
    request.schematic = b.schematic;
    request.library = &board_library();
    request.mcus.push_back({});
    sim::SimulationSession session;
    std::string error;
    CHECK(!session.start(request, error));
    CHECK(error.find("Transient mode") != std::string::npos);
}

TEST_CASE("session: a GPIO output drives the circuit through its pin model")
{
#ifndef SIMNODUS_TEST_RENODE
    std::printf("  skipped: no Renode in this build\n");
#else
    REQUIRE(test::engine() != nullptr);
    test::CircuitBuilder b(board_library());
    b.add("bluepill", {{"G1", "0"}, {"PA0", "OUT"}}, {{"firmware", "firmware/blink/blink.elf"}});
    b.add("resistor", {{"1", "OUT"}, {"2", "0"}}, {{"R", "1k"}});
    auto settings = b.schematic.settings();
    settings.speed = 0.0;
    settings.segment = 0.5;
    settings.max_step = 1e-4;
    b.schematic.set_settings(settings);
    sim::RunRequest request;
    request.schematic = b.schematic;
    request.library = &board_library();
    request.vectors = {"OUT"};
    std::string error;
    REQUIRE(sim::build_mcu_requests(b.schematic, board_library(), environment(), request.mcus, error));
    sim::SimulationSession session;
    REQUIRE(session.start(request, error));
    const auto begin = std::chrono::steady_clock::now();
    while (session.status().sim_time < 1.1) {
        REQUIRE(session.active());
        REQUIRE(std::chrono::steady_clock::now() - begin < 300s);
        std::this_thread::sleep_for(20ms);
    }
    session.stop();
    const auto samples = session.samples("out");
    REQUIRE(samples.size() > 100);
    // Edges where the voltage crosses half the supply.
    std::vector<std::pair<double, bool>> edges;
    double high = 0.0;
    for (std::size_t i = 1; i < samples.size(); ++i) {
        const bool before = samples[i - 1].value > 1.65;
        const bool after = samples[i].value > 1.65;
        if (before != after) {
            edges.emplace_back(samples[i].time, after);
        }
        if (after) {
            high = std::max(high, samples[i].value);
        }
    }
    for (const auto& [t, rising] : edges) {
        std::printf("  %s edge at %.6f s\n", rising ? "rising" : "falling", t);
    }
    // 3.3 V through 40 Ohm into 1 kOhm.
    std::printf("  high level %.4f V\n", high);
    CHECK(std::abs(high - 3.3 * 1000.0 / 1040.0) < 0.01);
    REQUIRE(edges.size() == 4);
    for (std::size_t i = 0; i < edges.size(); ++i) {
        const double expected = 0.25 * static_cast<double>(i + 1);
        CHECK(edges[i].second == (i % 2 == 0));
        // Renode's SysTick is 125 ppm fast; events have its 100 us quantum; plus one step.
        CHECK(std::abs(edges[i].first - expected) < 2e-4 * expected + 3e-4);
        if (on_native()) {
            // Exact SysTick and exact edge times: only the circuit step (0.1 ms) remains.
            CHECK(std::abs(edges[i].first - expected) <= 1.2e-4);
        }
    }
#endif
}

TEST_CASE("session: coupled runs with the same circuit inputs repeat exactly on the own emulator")
{
#ifndef SIMNODUS_TEST_RENODE
    std::printf("  skipped: no Renode in this build\n");
#else
    if (!on_native()) {
        std::printf("  skipped: determinism is a property of the own emulator's window coupling (SN-173)\n");
        return;
    }
    REQUIRE(test::engine() != nullptr);
    // A 25 Hz square wave on PA1 through 1 kOhm: every falling edge runs the EXTI1
    // handler of button_irq, which toggles PA0. Inputs reach the firmware at
    // window boundaries, so the PA0 log must be the same in every run. (With the
    // earlier rule, "at the circuit time or at once", this scenario happened to
    // repeat as well, because the idle firmware always waits at its limit; the
    // window rule guarantees it by construction.)
    auto run_once = [] {
        test::CircuitBuilder b(board_library());
        b.add("bluepill", {{"G1", "0"}, {"PA0", "OUT"}, {"PA1", "IN"}}, {{"firmware", "firmware/button_irq/button_irq.elf"}});
        b.add("vpulse", {{"+", "SRC"}, {"-", "0"}}, {{"Vlow", "0"}, {"Vhigh", "3.3"}, {"f", "25"}, {"duty", "50"}, {"rise", "10u"}});
        b.add("resistor", {{"1", "SRC"}, {"2", "IN"}}, {{"R", "1k"}});
        b.add("resistor", {{"1", "OUT"}, {"2", "0"}}, {{"R", "1k"}});
        auto settings = b.schematic.settings();
        settings.speed = 0; // as fast as possible: pacing must not matter
        settings.segment = 0.5;
        settings.max_step = 1e-4;
        settings.mcu_quantum = 1e-3;
        b.schematic.set_settings(settings);
        sim::RunRequest request;
        request.schematic = b.schematic;
        request.library = &board_library();
        request.vectors = {"OUT", "IN"};
        std::string error;
        std::vector<std::pair<std::uint64_t, bool>> log;
        if (!sim::build_mcu_requests(b.schematic, board_library(), environment(), request.mcus, error)) {
            return log;
        }
        sim::SimulationSession session;
        if (!session.start(request, error)) {
            return log;
        }
        const auto begin = std::chrono::steady_clock::now();
        while (session.status().sim_time < 0.5 && session.active()
            && std::chrono::steady_clock::now() - begin < 120s) {
            std::this_thread::sleep_for(5ms);
        }
        const model::Id board = session.netlist().externals.front().component;
        log = session.mcu_pin_log(board, "PA0", 0).changes;
        session.stop();
        // Compare only what both runs certainly reached.
        while (!log.empty() && log.back().first > 450000000ull) {
            log.pop_back();
        }
        return log;
    };
    const auto first = run_once();
    const auto second = run_once();
    std::printf("  PA0 changes in 0.45 s: %zu and %zu\n", first.size(), second.size());
    REQUIRE(first.size() >= 10); // a falling edge every 40 ms: 11 by 0.45 s
    CHECK(first == second);
    for (std::size_t i = 0; i < first.size() && i < second.size(); ++i) {
        if (first[i] != second[i]) {
            std::printf("  first difference #%zu: %llu vs %llu ns\n", i, static_cast<unsigned long long>(first[i].first),
                static_cast<unsigned long long>(second[i].first));
            break;
        }
    }
#endif
}

TEST_CASE("session: measured cost of the coupling window on the own emulator")
{
#ifndef SIMNODUS_TEST_RENODE
    std::printf("  skipped: no Renode in this build\n");
#else
    if (!on_native()) {
        std::printf("  skipped: measures the own emulator's window L (EQ-3)\n");
        return;
    }
    REQUIRE(test::engine() != nullptr);
    // A 100 Hz square wave on PA1 (EXTI1 toggles PA0) for 2 s of circuit time at
    // maximum speed, for several windows L. Reported, not asserted: it informed
    // the default window of 100 us (MCU_EMULATOR.md EQ-3; measured before the cap,
    // a 1 ms window ran at 2.48x real time, 100 us at 2.40x, 20 us at 2.32x).
    for (const double window : {1e-4, 2e-5}) {
        test::CircuitBuilder b(board_library());
        b.add("bluepill", {{"G1", "0"}, {"PA0", "OUT"}, {"PA1", "IN"}}, {{"firmware", "firmware/button_irq/button_irq.elf"}});
        b.add("vpulse", {{"+", "SRC"}, {"-", "0"}}, {{"Vlow", "0"}, {"Vhigh", "3.3"}, {"f", "100"}, {"duty", "50"}, {"rise", "10u"}});
        b.add("resistor", {{"1", "SRC"}, {"2", "IN"}}, {{"R", "1k"}});
        b.add("resistor", {{"1", "OUT"}, {"2", "0"}}, {{"R", "1k"}});
        auto settings = b.schematic.settings();
        settings.speed = 0;
        settings.segment = 0.5;
        settings.max_step = 1e-4;
        settings.mcu_quantum = window;
        b.schematic.set_settings(settings);
        sim::RunRequest request;
        request.schematic = b.schematic;
        request.library = &board_library();
        request.vectors = {"OUT", "IN"};
        std::string error;
        REQUIRE(sim::build_mcu_requests(b.schematic, board_library(), environment(), request.mcus, error));
        sim::SimulationSession session;
        REQUIRE(session.start(request, error));
        const auto begin = std::chrono::steady_clock::now();
        while (session.status().sim_time < 2.0 && session.active() && std::chrono::steady_clock::now() - begin < 300s) {
            std::this_thread::sleep_for(2ms);
        }
        const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - begin).count();
        const model::Id board = session.netlist().externals.front().component;
        const auto status = session.mcu_status(board);
        const auto toggles = session.mcu_pin_log(board, "PA0", 0).changes.size();
        session.stop();
        REQUIRE(status.has_value());
        std::printf("  window %6.0f us: 2 s of circuit time in %.2f s wall (%.2fx real time), largest input delay %.0f us, "
                    "%zu PA0 toggles\n",
            window * 1e6, seconds, 2.0 / seconds, static_cast<double>(status->max_input_delay_ns) / 1000.0, toggles);
        CHECK(toggles >= 190); // 200 falling edges in 2 s
        CHECK(static_cast<double>(status->max_input_delay_ns) <= window * 1e9 + 1);
    }
#endif
}

TEST_CASE("session: a push button on the schematic triggers the firmware interrupt")
{
#ifndef SIMNODUS_TEST_RENODE
    std::printf("  skipped: no Renode in this build\n");
#else
    REQUIRE(test::engine() != nullptr);
    test::CircuitBuilder b(board_library());
    b.add("bluepill", {{"G1", "0"}, {"PA0", "OUT"}, {"PA1", "BTN"}}, {{"firmware", "firmware/button_irq/button_irq.elf"}});
    const model::Id button = b.add("button_no", {{"1", "BTN"}, {"2", "0"}});
    b.add("resistor", {{"1", "OUT"}, {"2", "0"}}, {{"R", "1k"}});
    auto settings = b.schematic.settings();
    settings.speed = 1.0;
    settings.segment = 0.5;
    settings.max_step = 1e-4;
    settings.mcu_quantum = 1e-3;
    b.schematic.set_settings(settings);
    sim::RunRequest request;
    request.schematic = b.schematic;
    request.library = &board_library();
    request.vectors = {"OUT", "BTN"};
    std::string error;
    REQUIRE(sim::build_mcu_requests(b.schematic, board_library(), environment(), request.mcus, error));
    sim::SimulationSession session;
    REQUIRE(session.start(request, error));
    const auto begin = std::chrono::steady_clock::now();
    auto wait_until = [&](auto condition) {
        while (!condition()) {
            REQUIRE(session.active());
            REQUIRE(std::chrono::steady_clock::now() - begin < 300s);
            std::this_thread::sleep_for(5ms);
        }
    };
    wait_until([&] { return session.status().sim_time > 0.2; });
    // The internal pull-up (40 kOhm) holds the open button's pin high; the LED output is low.
    CHECK(session.latest("btn").value_or(0) > 3.2);
    CHECK(session.latest("out").value_or(9) < 0.1);

    auto press = [&](bool pressed, bool expected_out) {
        session.set_control(button, pressed ? 1.0 : 0.0);
        const double from = session.status().sim_time;
        wait_until([&] { return session.status().sim_time > from + 0.1; });
        CHECK((session.latest("out").value_or(0) > 1.65) == expected_out);
    };
    press(true, true);   // first press: interrupt turns the LED on
    press(false, true);  // release: no change
    press(true, false);  // second press: off
    press(false, false);
    session.stop();
    const auto status = session.mcu_status(session.netlist().externals.front().component);
    REQUIRE(status.has_value());
    std::printf("  largest circuit-to-firmware input delay: %llu us\n",
        static_cast<unsigned long long>(status->max_input_delay_ns / 1000));
    CHECK(status->max_input_delay_ns / 1000 <= 2 * 1000 + 100); // two quanta plus one circuit step
    if (on_native()) {
        CHECK(status->max_input_delay_ns <= 1000000); // at most one window (the 1 ms quantum)
    }

    // Delay from the button's falling edge to the output's rising edge, in circuit time.
    const auto btn = session.samples("btn");
    const auto out = session.samples("out");
    double pressed_at = -1;
    double lit_at = -1;
    for (std::size_t i = 1; i < btn.size() && pressed_at < 0; ++i) {
        if (btn[i - 1].value > 1.16 && btn[i].value <= 1.16) {
            pressed_at = btn[i].time;
        }
    }
    for (std::size_t i = 1; i < out.size() && lit_at < 0; ++i) {
        if (out[i - 1].value < 1.65 && out[i].value >= 1.65) {
            lit_at = out[i].time;
        }
    }
    std::printf("  button edge %.6f s, LED edge %.6f s, response %.0f us\n", pressed_at, lit_at,
        (lit_at - pressed_at) * 1e6);
    REQUIRE(pressed_at > 0);
    REQUIRE(lit_at > pressed_at);
    CHECK(lit_at - pressed_at < 2.5e-3); // two quanta, Renode quantum and steps
    if (on_native()) {
        CHECK(lit_at - pressed_at <= 1.0e-3 + 2.5e-4); // one window plus the circuit steps of both edges
    }
#endif
}

TEST_CASE("session: lesson 2 counts bounces without the RC filter and one edge with it")
{
#ifndef SIMNODUS_TEST_RENODE
    std::printf("  skipped: no Renode in this build\n");
#else
    REQUIRE(test::engine() != nullptr);
    const auto loaded = io::load_project(std::filesystem::path(SIMNODUS_TEST_EXAMPLES_DIR) / "11_Lesson2_button_and_EXTI.snx");
    REQUIRE(loaded.schematic.has_value());
    model::Id board = model::no_id;
    model::Id button = model::no_id;
    model::Id debounce = model::no_id;
    for (const auto& c : loaded.schematic->components()) {
        board = c.designator == "U1" ? c.id : board;
        button = c.designator == "SW1" ? c.id : button;
        debounce = c.designator == "SW2" ? c.id : debounce;
    }
    for (const bool filtered : {false, true}) {
        sim::RunRequest request;
        request.schematic = *loaded.schematic;
        request.library = &board_library();
        request.vectors = {"btn"};
        std::string error;
        REQUIRE(sim::build_mcu_requests(request.schematic, board_library(), environment(), request.mcus, error));
        sim::SimulationSession session;
        REQUIRE(session.start(request, error));
        const auto begin = std::chrono::steady_clock::now();
        auto wait_until = [&](double t) {
            while (session.status().sim_time < t) {
                REQUIRE(session.active());
                REQUIRE(std::chrono::steady_clock::now() - begin < 300s);
                std::this_thread::sleep_for(5ms);
            }
        };
        wait_until(0.1);
        auto count_edges = [&] {
            std::string text;
            session.mcu_uart_received(board, "usart1", 0, text);
            const auto last = text.rfind("edges: ");
            return last == std::string::npos ? 0 : std::atoi(text.c_str() + last + 7);
        };
        if (filtered) {
            // Closing SW2 connects the discharged capacitor: the input falls once, then
            // the capacitor charges through the 10 kOhm pull-up (tau = 10 ms).
            session.set_control(debounce, 1.0);
            wait_until(session.status().sim_time + 0.1);
        }
        const int before = count_edges();
        session.set_control(button, 1.0);
        wait_until(session.status().sim_time + 0.2);
        session.set_control(button, 0.0);
        wait_until(session.status().sim_time + 0.2);
        const int edges = count_edges() - before;
        // The button node never leaves the supply range (trapezoidal ringing, ADR 0087).
        for (const auto& s : session.samples("btn")) {
            CHECK(s.value > -0.1 && s.value < 3.4);
        }
        session.stop();
        std::printf("  %s: %d falling edges from one press and release\n", filtered ? "with RC filter" : "without filter",
            edges);
        if (filtered) {
            CHECK(edges == 1);
        } else {
            CHECK(edges > 2); // bounces on press and on release
        }
    }
#endif
}

// Long run for gate G1 (memory flat over an hour). Not part of the regular suites:
// engine-native filters "session:", and with Renode the case returns at once.
//   engine_tests <ngspice> <scripts> "soak:" native
TEST_CASE("soak: one hour of example 07 on the own emulator keeps memory flat")
{
#ifndef SIMNODUS_TEST_RENODE
    std::printf("  skipped: no Renode in this build\n");
#else
    if (test_mcu_engine != "native") {
        std::printf("  skipped: run explicitly with the native engine\n");
        return;
    }
    REQUIRE(test::engine() != nullptr);
    const auto loaded = io::load_project(std::filesystem::path(SIMNODUS_TEST_EXAMPLES_DIR) / "07_Blue_Pill_blink.snx");
    REQUIRE(loaded.schematic.has_value());
    sim::RunRequest request;
    request.schematic = *loaded.schematic;
    auto settings = request.schematic.settings();
    settings.speed = 0.0; // the hour as fast as the machine allows
    settings.segment = test_soak_segment;
    request.schematic.set_settings(settings);
    request.library = &board_library();
    request.vectors = {"pa0"};
    std::string error;
    REQUIRE(sim::build_mcu_requests(request.schematic, board_library(), environment(), request.mcus, error));
    sim::SimulationSession session;
    REQUIRE(session.start(request, error));
    const auto begin = std::chrono::steady_clock::now();
    std::uint64_t warm = 0;
    double next_report = 60.0;
    while (session.status().sim_time < 3600.0) {
        REQUIRE(session.active());
        REQUIRE(std::chrono::steady_clock::now() - begin < 7200s);
        std::this_thread::sleep_for(200ms);
        const double t = session.status().sim_time;
        if (t >= next_report) {
            const auto memory = sim::current_process_memory();
            if (warm == 0) {
                warm = memory.private_bytes;
            }
            std::printf("  %6.0f s simulated after %6.0f s: private %.1f MB (%+.1f MB since 60 s)\n", t,
                std::chrono::duration<double>(std::chrono::steady_clock::now() - begin).count(),
                static_cast<double>(memory.private_bytes) / 1e6,
                (static_cast<double>(memory.private_bytes) - static_cast<double>(warm)) / 1e6);
            std::fflush(stdout);
            next_report += 300.0;
        }
    }
    const auto end = sim::current_process_memory();
    session.stop();
    CHECK(end.private_bytes < warm + 10000000); // under 10 MB growth over the hour (G1)
#endif
}

// Reference for the soak above: the same LED circuit driven by a 2 Hz pulse
// source instead of the board, for 1500 s of circuit time (memory growth of the
// analog side alone). Run explicitly: engine_tests ... "soak:" native
TEST_CASE("soak: the analog circuit alone, for comparison")
{
    if (test_mcu_engine != "native") {
        std::printf("  skipped: run explicitly with \"soak:\" native\n");
        return;
    }
    REQUIRE(test::engine() != nullptr);
    test::CircuitBuilder b(board_library());
    b.add("vpulse", {{"+", "PA0"}, {"-", "0"}}, {{"Vlow", "0"}, {"Vhigh", "3.3"}, {"f", "2"}, {"duty", "50"}, {"rise", "1u"}});
    b.add("resistor", {{"1", "PA0"}, {"2", "A"}}, {{"R", "330"}});
    b.add("led", {{"A", "A"}, {"K", "0"}});
    auto settings = b.schematic.settings();
    settings.speed = 0.0;
    settings.segment = test_soak_segment;
    b.schematic.set_settings(settings);
    sim::RunRequest request;
    request.schematic = b.schematic;
    request.library = &board_library();
    request.vectors = {"pa0"};
    std::string error;
    sim::SimulationSession session;
    REQUIRE(session.start(request, error));
    const auto begin = std::chrono::steady_clock::now();
    std::uint64_t warm = 0;
    double next_report = 60.0;
    while (session.status().sim_time < 1500.0) {
        REQUIRE(session.active());
        REQUIRE(std::chrono::steady_clock::now() - begin < 7200s);
        std::this_thread::sleep_for(200ms);
        const double t = session.status().sim_time;
        if (t >= next_report) {
            const auto memory = sim::current_process_memory();
            if (warm == 0) {
                warm = memory.private_bytes;
            }
            std::printf("  %6.0f s simulated after %6.0f s: private %.1f MB (%+.1f MB since 60 s)\n", t,
                std::chrono::duration<double>(std::chrono::steady_clock::now() - begin).count(),
                static_cast<double>(memory.private_bytes) / 1e6,
                (static_cast<double>(memory.private_bytes) - static_cast<double>(warm)) / 1e6);
            std::fflush(stdout);
            next_report += 300.0;
        }
    }
    session.stop();
}

TEST_CASE("session: lesson 1 filters the 1 kHz square wave as the RC formula predicts")
{
#ifndef SIMNODUS_TEST_RENODE
    std::printf("  skipped: no Renode in this build\n");
#else
    REQUIRE(test::engine() != nullptr);
    const auto loaded = io::load_project(std::filesystem::path(SIMNODUS_TEST_EXAMPLES_DIR) / "10_Lesson1_GPIO_output.snx");
    REQUIRE(loaded.schematic.has_value());
    sim::RunRequest request;
    request.schematic = *loaded.schematic;
    auto settings = request.schematic.settings();
    settings.speed = 0.0;
    settings.max_step = 1e-5; // resolve the 0.5 ms half periods finely
    request.schematic.set_settings(settings);
    request.library = &board_library();
    request.vectors = {"pa1", "rc"};
    std::string error;
    REQUIRE(sim::build_mcu_requests(request.schematic, board_library(), environment(), request.mcus, error));
    sim::SimulationSession session;
    REQUIRE(session.start(request, error));
    const auto begin = std::chrono::steady_clock::now();
    while (session.status().sim_time < 0.1) {
        REQUIRE(session.active());
        REQUIRE(std::chrono::steady_clock::now() - begin < 300s);
        std::this_thread::sleep_for(10ms);
    }
    session.stop();
    // Steady state over the last 20 ms: 1 kHz square wave, RC peak-to-peak 3.3 * tanh(T / 4 tau).
    const auto pa1 = session.samples("pa1");
    const auto rc = session.samples("rc");
    REQUIRE(!pa1.empty() && !rc.empty());
    const double from = pa1.back().time - 0.02;
    int rising = 0;
    for (std::size_t i = 1; i < pa1.size(); ++i) {
        rising += pa1[i].time >= from && pa1[i - 1].value < 1.65 && pa1[i].value >= 1.65 ? 1 : 0;
    }
    double low = 9;
    double high = -9;
    for (const auto& s : rc) {
        if (s.time >= from) {
            low = std::min(low, s.value);
            high = std::max(high, s.value);
        }
    }
    // The pin's 40 Ohm output resistance adds to the 10 kOhm: tau = 10040 * 100 nF.
    const double expected = 3.3 * std::tanh(1e-3 / (4 * 10040.0 * 100e-9));
    std::printf("  %d rising edges in 20 ms, RC %.3f .. %.3f V, peak-to-peak %.3f V (expected %.3f V)\n", rising, low,
        high, high - low, expected);
    if (rising < 19 || rising > 21 || std::abs((high - low) - expected) >= 0.02) {
        // Diagnostics: the circuit's PA1 edges next to the times Renode reported.
        int shown = 0;
        for (std::size_t i = 1; i < pa1.size() && shown < 40; ++i) {
            if (pa1[i].time >= from && (pa1[i - 1].value < 1.65) != (pa1[i].value < 1.65)) {
                std::printf("  circuit PA1 edge %.6f s -> %.2f V\n", pa1[i].time, pa1[i].value);
                ++shown;
            }
        }
        const auto log = session.mcu_pin_log(session.netlist().externals.front().component, "PA1",
            static_cast<std::uint64_t>(from * 1e9));
        for (std::size_t i = 0; i < log.changes.size() && i < 40; ++i) {
            std::printf("  renode PA1 %llu us -> %d\n", static_cast<unsigned long long>(log.changes[i].first / 1000),
                log.changes[i].second ? 1 : 0);
        }
    }
    CHECK(rising >= 19 && rising <= 21);
    CHECK(std::abs((high - low) - expected) < 0.02); // edges on the 100 us Renode quantum
    CHECK(std::abs((high + low) / 2 - 1.65) < 0.05);
#endif
}
