// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// A Blue Pill in an interactive run (SN-131): ngspice and Renode together.
// Skipped (and reported) when the build has no Renode.
#include "test_support.hpp"

#include "circuit_builder.hpp"
#include "engine_fixture.hpp"
#include "sim/simulation_session.hpp"

#include <chrono>
#include <filesystem>
#include <set>
#include <thread>

using namespace simnodus;
using namespace std::chrono_literals;

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
    return {SIMNODUS_TEST_EXAMPLES_DIR, std::filesystem::path(SIMNODUS_TEST_RENODE),
        std::filesystem::path(SIMNODUS_TEST_MCU_DIR)};
}
#endif

} // namespace

TEST_CASE("session: boards without firmware or engine are explained")
{
    test::CircuitBuilder b(board_library());
    b.add("bluepill", {{"G1", "0"}});
    std::vector<sim::McuRequest> requests;
    std::string error;
    CHECK(!sim::build_mcu_requests(b.schematic, board_library(), {"."}, requests, error));
    CHECK(error.find("no firmware") != std::string::npos);

    test::CircuitBuilder missing(board_library());
    missing.add("bluepill", {{"G1", "0"}}, {{"firmware", "does/not/exist.elf"}});
    error.clear();
    CHECK(!sim::build_mcu_requests(missing.schematic, board_library(), {"."}, requests, error));
    CHECK(error.find("does not exist") != std::string::npos);

    test::CircuitBuilder no_engine(board_library());
    no_engine.add("bluepill", {{"G1", "0"}}, {{"firmware", "firmware/blink/blink.elf"}});
    error.clear();
    CHECK(!sim::build_mcu_requests(no_engine.schematic, board_library(), {SIMNODUS_TEST_EXAMPLES_DIR}, requests, error));
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
        REQUIRE(std::chrono::steady_clock::now() - begin < 60s);
        std::this_thread::sleep_for(10ms);
    }
    std::printf("  circuit started after %.1f s\n",
        std::chrono::duration<double>(std::chrono::steady_clock::now() - begin).count());
    CHECK(saw_starting);

    // Watch PC13 over 1.6 s of virtual time: the firmware toggles it every 500 ms.
    std::set<bool> levels;
    std::uint64_t worst_lead = 0;
    std::uint64_t worst_lag = 0;
    while (session.status().sim_time < 1.6) {
        REQUIRE(session.active());
        REQUIRE(std::chrono::steady_clock::now() - begin < 120s);
        const auto status = session.mcu_status(board);
        REQUIRE(status.has_value());
        REQUIRE(status->state == mcu::McuState::Running);
        const double analog = session.status().sim_time;
        const auto analog_us = static_cast<std::uint64_t>(analog * 1e6);
        if (status->time_us > analog_us) {
            worst_lead = std::max(worst_lead, status->time_us - analog_us);
        } else {
            worst_lag = std::max(worst_lag, analog_us - status->time_us);
        }
        const auto pins = session.mcu_pins(board);
        if (status->time_us > 1000) { // the reset handler has configured the pins
            levels.insert(pins.at("PC13").level);
            CHECK(pins.at("PC13").mode == mcu::PinMode::Output);
        }
        std::this_thread::sleep_for(20ms);
    }
    std::printf("  firmware ahead of the circuit by at most %llu us, behind by at most %llu us\n",
        static_cast<unsigned long long>(worst_lead), static_cast<unsigned long long>(worst_lag));
    CHECK(levels.size() == 2);
    CHECK(worst_lead <= request.mcu_lead_us);
    // The status is sampled between the circuit's and the firmware's updates; allow one run chunk.
    CHECK(worst_lag <= request.mcu_lead_us + 10000);
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
