// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// Simulation session against the real engine (SN-117, SN-118, SN-121).
#include "test_support.hpp"

#include "circuit_builder.hpp"
#include "engine_fixture.hpp"
#include "sim/simulation_session.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <thread>

using namespace simnodus;
using namespace std::chrono_literals;

namespace {

library::ComponentLibrary& components()
{
    static library::ComponentLibrary library = [] {
        library::ComponentLibrary l;
        l.load_directory(SIMNODUS_TEST_LIBRARY_DIR);
        return l;
    }();
    return library;
}

// Waits until simulated time passes `t` (or the run ends); false on timeout.
bool wait_for_time(const sim::SimulationSession& session, double t, std::chrono::milliseconds timeout = 20s)
{
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        const auto status = session.status();
        if (status.sim_time >= t || !session.active()) {
            return status.sim_time >= t;
        }
        std::this_thread::sleep_for(5ms);
    }
    return false;
}

struct ButtonCircuit {
    test::CircuitBuilder builder{components()};
    model::Id button = model::no_id;

    explicit ButtonCircuit(double speed, double segment = 0.25)
    {
        builder.add("vdc", {{"+", "VCC"}, {"-", "0"}}, {{"V", "5"}});
        button = builder.add("button_no", {{"1", "VCC"}, {"2", "SW"}});
        builder.add("resistor", {{"1", "SW"}, {"2", "CAP"}}, {{"R", "1k"}});
        builder.add("capacitor", {{"1", "CAP"}, {"2", "0"}}, {{"C", "10u"}}); // tau = 10 ms
        builder.add("resistor", {{"1", "CAP"}, {"2", "0"}}, {{"R", "100k"}});
        auto settings = builder.schematic.settings();
        settings.speed = speed;
        settings.segment = segment;
        settings.max_step = 1e-4;
        builder.schematic.set_settings(settings);
    }

    sim::RunRequest request() const
    {
        sim::RunRequest r;
        r.schematic = builder.schematic;
        r.library = &components();
        r.vectors = {"CAP", "SW"};
        r.ring_capacity = 50000;
        return r;
    }
};

} // namespace

TEST_CASE("session: a button charges the capacitor in an interactive run")
{
    REQUIRE(test::engine() != nullptr);
    ButtonCircuit circuit(0.0); // maximum speed
    sim::SimulationSession session;
    std::string error;
    REQUIRE(session.start(circuit.request(), error));
    REQUIRE(wait_for_time(session, 0.3));
    CHECK(session.latest("cap").value_or(-1) < 0.01);
    session.set_control(circuit.button, 1.0);
    const double pressed_at = session.status().sim_time;
    REQUIRE(wait_for_time(session, pressed_at + 0.2));
    const double charged = session.latest("cap").value_or(0);
    CHECK(charged > 4.9 * 100.0 / 101.0 && charged < 5.0);
    session.set_control(circuit.button, 0.0);
    const double released_at = session.status().sim_time;
    REQUIRE(wait_for_time(session, released_at + 1.2)); // discharge through 100k: tau = 1 s
    // At maximum speed the run moves on while the test reads, so check the
    // recorded sample 1.2 s after the release: e^-1.2 of the charged value.
    double later = -1;
    for (const auto& s : session.samples("CAP")) {
        if (s.time >= released_at + 1.2) {
            later = s.value;
            break;
        }
    }
    std::printf("  charged %.4f V, 1.2 s after release %.4f V (expected %.4f)\n", charged, later,
        charged * std::exp(-1.2));
    CHECK(std::abs(later - charged * std::exp(-1.2)) < 0.05 * charged * std::exp(-1.2));
    const auto status = session.status();
    CHECK(status.segments >= 6);
    // Continuity across segment boundaries: no step larger than a normal one.
    const auto samples = session.samples("CAP");
    REQUIRE(samples.size() > 100);
    double worst_step = 0;
    for (std::size_t i = 1; i < samples.size(); ++i) {
        CHECK(samples[i].time > samples[i - 1].time);
        worst_step = std::max(worst_step, std::abs(samples[i].value - samples[i - 1].value));
    }
    std::printf("  %zu samples, %d segments, largest step %.3g V\n", samples.size(), status.segments, worst_step);
    CHECK(worst_step < 0.1); // 100 us steps on a 10 ms time constant: < 0.05 V
    session.stop();
    CHECK(session.status().state == sim::RunState::Stopped);
}

TEST_CASE("session: an open push button does not charge a capacitor")
{
    // Owner report 2026-10-14: a capacitor behind an open button slowly charged.
    REQUIRE(test::engine() != nullptr);
    for (const char* layout : {"parallel", "series"}) {
        test::CircuitBuilder b(components());
        b.add("vdc", {{"+", "VCC"}, {"-", "0"}}, {{"V", "5"}});
        b.add("button_no", {{"1", "VCC"}, {"2", "A"}});
        if (std::string(layout) == "parallel") {
            b.add("capacitor", {{"1", "A"}, {"2", "0"}}, {{"C", "100n"}});
            b.add("voltmeter", {{"+", "A"}, {"-", "0"}});
        } else {
            b.add("capacitor", {{"1", "A"}, {"2", "B"}}, {{"C", "100n"}});
            b.add("voltmeter", {{"+", "B"}, {"-", "0"}});
        }
        auto settings = b.schematic.settings();
        settings.speed = 0;
        b.schematic.set_settings(settings);
        sim::RunRequest request;
        request.schematic = b.schematic;
        request.library = &components();
        request.vectors = {"A", "B"};
        sim::SimulationSession session;
        std::string error;
        REQUIRE(session.start(request, error));
        REQUIRE(wait_for_time(session, 20.0, 60s));
        const double a = session.latest("a").value_or(-1);
        session.stop();
        std::printf("  %s: V(A) after 20 s with the button open = %.3g V\n", layout, a);
        CHECK(std::abs(a) < 1e-3);
    }
}

TEST_CASE("session: periodic sources keep their phase across segments")
{
    REQUIRE(test::engine() != nullptr);
    test::CircuitBuilder b(components());
    b.add("vpulse", {{"+", "P"}, {"-", "0"}}, {{"f", "3"}, {"duty", "50"}, {"rise", "10u"}});
    b.add("resistor", {{"1", "P"}, {"2", "0"}});
    auto settings = b.schematic.settings();
    settings.speed = 0;
    settings.segment = 0.25; // not a multiple of the 333 ms period
    b.schematic.set_settings(settings);
    sim::RunRequest request;
    request.schematic = b.schematic;
    request.library = &components();
    request.vectors = {"P"};
    sim::SimulationSession session;
    std::string error;
    REQUIRE(session.start(request, error));
    REQUIRE(wait_for_time(session, 2.0));
    session.stop();
    int mismatches = 0;
    int checked = 0;
    for (const auto& s : session.samples("P")) {
        const double phase = std::fmod(s.time, 1.0 / 3.0) * 3.0; // 0..1
        if (std::abs(phase - 0.5) < 1e-3 || phase < 1e-3 || phase > 1 - 1e-3) {
            continue; // near an edge
        }
        ++checked;
        const double expected = phase < 0.5 ? 5.0 : 0.0;
        mismatches += std::abs(s.value - expected) > 1e-6 ? 1 : 0;
    }
    std::printf("  %d samples checked, %d mismatches\n", checked, mismatches);
    CHECK(checked > 1000);
    CHECK(mismatches == 0);
}

TEST_CASE("session: paced run follows real time and pauses")
{
    REQUIRE(test::engine() != nullptr);
    ButtonCircuit circuit(1.0, 1.0);
    sim::SimulationSession session;
    std::string error;
    REQUIRE(session.start(circuit.request(), error));
    std::this_thread::sleep_for(1200ms);
    auto status = session.status();
    std::printf("  after 1.2 s wall: t = %.3f s, factor %.3f\n", status.sim_time, status.achieved_factor);
    CHECK(status.sim_time > 1.0 && status.sim_time < 1.3);
    CHECK(status.achieved_factor > 0.8 && status.achieved_factor < 1.2);
    session.pause();
    std::this_thread::sleep_for(150ms);
    CHECK(session.status().state == sim::RunState::Paused);
    const double paused_time = session.status().sim_time;
    std::this_thread::sleep_for(400ms);
    CHECK(std::abs(session.status().sim_time - paused_time) < 1e-9);
    session.resume();
    std::this_thread::sleep_for(400ms);
    CHECK(session.status().state == sim::RunState::Running);
    CHECK(session.status().sim_time > paused_time + 0.25);
    // Stopping ends a paced run promptly.
    const auto before = std::chrono::steady_clock::now();
    session.stop();
    const double took = std::chrono::duration<double>(std::chrono::steady_clock::now() - before).count();
    std::printf("  stop took %.3f s\n", took);
    CHECK(took < 1.0);
    CHECK(!session.active());
}

TEST_CASE("session: transient mode keeps the complete result")
{
    REQUIRE(test::engine() != nullptr);
    test::CircuitBuilder b(components());
    b.add("vdc", {{"+", "IN"}, {"-", "0"}}, {{"V", "3.3"}});
    b.add("resistor", {{"1", "IN"}, {"2", "OUT"}});
    b.add("capacitor", {{"1", "OUT"}, {"2", "0"}}, {{"C", "1u"}});
    auto settings = b.schematic.settings();
    settings.mode = model::SimulationMode::Transient;
    settings.transient_stop = 5e-3;
    settings.transient_max_step = 1e-6;
    b.schematic.set_settings(settings);
    sim::RunRequest request;
    request.schematic = b.schematic;
    request.library = &components();
    request.vectors = {"OUT"};
    sim::SimulationSession session;
    std::string error;
    REQUIRE(session.start(request, error));
    for (int i = 0; i < 400 && session.active(); ++i) {
        std::this_thread::sleep_for(10ms);
    }
    const auto status = session.status();
    CHECK(status.state == sim::RunState::Finished);
    CHECK(status.progress > 0.999);
    const auto result = session.transient_result();
    REQUIRE(!result.time.empty());
    REQUIRE(result.vectors.count("out"));
    const auto& out = result.vectors.at("out");
    REQUIRE(out.size() == result.time.size());
    double worst = 0;
    for (std::size_t i = 0; i < out.size(); ++i) {
        worst = std::max(worst, std::abs(out[i] - 3.3 * (1 - std::exp(-result.time[i] / 1e-3))));
    }
    std::printf("  %zu points, max error %.3g V\n", out.size(), worst);
    CHECK(worst < 0.0165);
    CHECK(std::abs(result.time.back() - 5e-3) < 1e-9);
}

TEST_CASE("session: circuits with errors do not start")
{
    REQUIRE(test::engine() != nullptr);
    test::CircuitBuilder b(components());
    b.add("resistor", {{"1", "A"}, {"2", "B"}}); // no ground
    sim::RunRequest request;
    request.schematic = b.schematic;
    request.library = &components();
    sim::SimulationSession session;
    std::string error;
    CHECK(!session.start(request, error));
    CHECK(!error.empty());
    CHECK(!session.diagnostics().empty());
    CHECK(!session.active());
}

TEST_CASE("session: a bouncing push button chatters for about 3 ms")
{
    REQUIRE(test::engine() != nullptr);
    for (const char* bounce : {"yes", "no"}) {
        test::CircuitBuilder b(components());
        b.add("vdc", {{"+", "VCC"}, {"-", "0"}}, {{"V", "3.3"}});
        const model::Id button = b.add("button_no", {{"1", "VCC"}, {"2", "SW"}}, {{"bounce", bounce}});
        b.add("resistor", {{"1", "SW"}, {"2", "0"}}, {{"R", "10k"}});
        auto settings = b.schematic.settings();
        settings.speed = 0.0;
        settings.segment = 0.25;
        settings.max_step = 1e-5;
        b.schematic.set_settings(settings);
        sim::RunRequest request;
        request.schematic = b.schematic;
        request.library = &components();
        request.vectors = {"SW"};
        request.ring_capacity = 200000;
        sim::SimulationSession session;
        std::string error;
        REQUIRE(session.start(request, error));
        REQUIRE(wait_for_time(session, 0.05));
        session.set_control(button, 1.0);
        const double pressed = session.status().sim_time;
        REQUIRE(wait_for_time(session, pressed + 0.02));
        session.stop();
        const auto samples = session.samples("sw");
        int edges = 0;
        double last_edge = 0.0;
        for (std::size_t i = 1; i < samples.size(); ++i) {
            if ((samples[i - 1].value > 1.65) != (samples[i].value > 1.65) && samples[i].time >= pressed - 1e-3) {
                ++edges;
                last_edge = samples[i].time;
            }
        }
        std::printf("  bounce %s: %d edges, the last %.2f ms after the press\n", bounce, edges, (last_edge - pressed) * 1e3);
        if (std::string(bounce) == "yes") {
            CHECK(edges == 9); // closes, then opens and closes again four times
            CHECK(last_edge - pressed < 3.5e-3);
        } else {
            CHECK(edges == 1);
        }
    }
}
