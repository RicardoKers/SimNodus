// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// Automation commands against the real engine (SN-215): runs, measurements,
// timed control changes and runs that stop at the asked time.
#include "test_support.hpp"

#include "automation/automation.hpp"

#include <chrono>
#include <cmath>
#include <filesystem>
#include <string>

using namespace simnodus;
using automation::json;

namespace {

const library::ComponentLibrary& parts()
{
    static library::ComponentLibrary library = [] {
        library::ComponentLibrary l;
        l.load_directory(SIMNODUS_TEST_LIBRARY_DIR);
        return l;
    }();
    return library;
}

struct Fixture {
    automation::HeadlessWorkspace workspace{parts()};
    automation::Automation automation{workspace};

    json ok(const std::string& command, json args = json::object())
    {
        const auto reply = automation.execute({{"command", command}, {"args", std::move(args)}});
        if (!reply.value("ok", false)) {
            std::printf("  %s failed: %s\n", command.c_str(), reply.dump().c_str());
        }
        return reply.value("ok", false) ? reply["result"] : json(nullptr);
    }

    // 100 Hz square wave into an RC low-pass (tau 1 ms), OUT labelled.
    void rc()
    {
        ok("component.add", {{"type", "vpulse"}, {"at", {0, 0}}, {"parameters", {{"f", "100"}}}});
        ok("component.add", {{"type", "resistor"}, {"at", {6, -6}}, {"rotation", 270}, {"parameters", {{"R", "1k"}}}});
        ok("component.add", {{"type", "capacitor"}, {"at", {12, 0}}, {"parameters", {{"C", "1u"}}}});
        ok("component.add", {{"type", "ground"}, {"attach", {{"to", "V1.-"}}}});
        ok("wire.connect", {{"from", "V1.+"}, {"to", "R1.1"}});
        ok("wire.connect", {{"from", "R1.2"}, {"to", "C1.1"}});
        ok("wire.connect", {{"from", "C1.2"}, {"to", "V1.-"}});
        ok("label.add", {{"name", "OUT"}, {"at", "C1.1"}});
    }
};

const json& signal(const json& result, const std::string& name)
{
    for (const auto& s : result["signals"]) {
        if (s["signal"] == name) {
            return s;
        }
    }
    static const json none;
    return none;
}

} // namespace

TEST_CASE("automation: a transient run measures the RC filter")
{
    Fixture f;
    f.rc();
    const auto result = f.ok("sim.run",
        {{"mode", "transient"}, {"duration", "40m"}, {"signals", {"V(OUT)", "I(R1.1)", "V(R1.1,R1.2)"}}, {"points", 9}});
    REQUIRE(result.is_object());
    CHECK(result["status"]["state"] == "Finished");
    const auto& out = signal(result, "V(OUT)");
    REQUIRE(out.is_object());
    CHECK(std::abs(out["frequency"].get<double>() - 100.0) < 1.0);
    CHECK(std::abs(out["mean"].get<double>() - 2.5) < 0.1);
    CHECK(out["max"].get<double>() > 4.9);    // 5 tau per half period
    CHECK(out["points"].size() == 9);
    const auto& current = signal(result, "I(R1.1)");
    REQUIRE(current.is_object());
    CHECK(current["max"].get<double>() > 4e-3); // 5 V / 1 kOhm at an edge
    const auto& across = signal(result, "V(R1.1,R1.2)");
    REQUIRE(across.is_object());
    // The voltage across R is R times its current.
    CHECK(std::abs(across["max"].get<double>() - 1000.0 * current["max"].get<double>()) < 0.05);
}

TEST_CASE("automation: the decade counter of example 23 counts across interactive segments")
{
    // The counter's state is in capacitors that every 1 s segment carries over;
    // its thresholds are shared nodes (tools/spice_levels.py). 2 Hz clock.
    Fixture f;
    REQUIRE(f.ok("project.open", {{"path", SIMNODUS_TEST_EXAMPLES_DIR "/23_Seven_segment_counter.snx"}}).is_object());
    const auto result = f.ok("sim.run", {{"mode", "interactive"}, {"duration", "4"},
        {"signals", {"V(U1.Q0)", "V(U1.Q1)", "V(U1.Q2)", "V(U1.Q3)"}}, {"points", 17}});
    REQUIRE(result.is_object());
    std::vector<int> counts;
    for (std::size_t k = 0; k < 17; ++k) {
        int count = 0;
        for (int bit = 0; bit < 4; ++bit) {
            const auto& s = signal(result, "V(U1.Q" + std::to_string(bit) + ")");
            REQUIRE(s.is_object() && s["points"].size() == 17);
            count |= s["points"][k][1].get<double>() > 2.5 ? 1 << bit : 0;
        }
        counts.push_back(count);
    }
    CHECK(counts == std::vector<int>({0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8}));
}

TEST_CASE("automation: interactive runs stop at the asked time and apply timed button presses")
{
    Fixture f;
    // 5 V -> push button -> 1k -> 10 uF to ground (tau 10 ms).
    f.ok("component.add", {{"type", "vdc"}, {"at", {0, 0}}, {"parameters", {{"V", "5"}}}});
    f.ok("component.add", {{"type", "button_no"}, {"at", {6, -6}}, {"rotation", 270}});
    f.ok("component.add", {{"type", "resistor"}, {"at", {14, -6}}, {"rotation", 270}, {"parameters", {{"R", "1k"}}}});
    f.ok("component.add", {{"type", "capacitor"}, {"at", {20, 0}}, {"parameters", {{"C", "10u"}}}});
    f.ok("component.add", {{"type", "ground"}, {"attach", {{"to", "V1.-"}}}});
    const auto pins = f.ok("circuit.inspect");
    REQUIRE(pins.is_object());
    std::string sw1, sw2;
    for (const auto& c : pins["components"]) {
        if (c["designator"] == "SW1") {
            sw1 = "SW1." + c["pins"][0]["name"].get<std::string>();
            sw2 = "SW1." + c["pins"][1]["name"].get<std::string>();
        }
    }
    REQUIRE(!sw1.empty());
    REQUIRE(f.ok("wire.connect", {{"from", "V1.+"}, {"to", sw1}}).is_object());
    REQUIRE(f.ok("wire.connect", {{"from", sw2}, {"to", "R1.1"}}).is_object());
    REQUIRE(f.ok("wire.connect", {{"from", "R1.2"}, {"to", "C1.1"}}).is_object());
    REQUIRE(f.ok("wire.connect", {{"from", "C1.2"}, {"to", "V1.-"}}).is_object());
    f.ok("label.add", {{"name", "CAP"}, {"at", "C1.1"}});
    REQUIRE(f.ok("circuit.check")["ok"] == true);

    const auto result = f.ok("sim.run", {{"mode", "interactive"}, {"duration", "30m"}, {"signals", {"V(CAP)"}},
        {"events", {{{"at", "10m"}, {"component", "SW1"}, {"value", "pressed"}}}}, {"points", 4}});
    REQUIRE(result.is_object());
    CHECK(std::abs(result["status"]["time"].get<double>() - 0.03) < 2e-4); // within one 100 us step
    REQUIRE(result["events"].size() == 1);
    const double applied = result["events"][0]["applied_at"].get<double>();
    CHECK(applied >= 0.01 && applied < 0.01 + 2e-4);
    const auto& cap = signal(result, "V(CAP)");
    REQUIRE(cap.is_object());
    // Charging from 10 ms: 5 (1 - e^-2) = 4.32 V at 30 ms; nothing before the press.
    CHECK(std::abs(cap["value"].get<double>() - 4.32) < 0.1);
    CHECK(std::abs(cap["points"][1][1].get<double>()) < 0.01); // 10 ms

    // Step by step: start, advance, press, advance, read.
    REQUIRE(f.ok("sim.start", {{"signals", {"V(CAP)"}}}).is_object());
    const auto at = f.ok("sim.advance", {{"to", "5m"}});
    REQUIRE(at.is_object());
    CHECK(at["status"]["state"] == "Paused");
    CHECK(std::abs(at["status"]["time"].get<double>() - 0.005) < 2e-4);
    REQUIRE(f.ok("sim.control", {{"component", "SW1"}, {"value", 1}}).is_object());
    REQUIRE(f.ok("sim.advance", {{"by", "10m"}}).is_object());
    const auto read = f.ok("sim.read");
    REQUIRE(read.is_object());
    // 10 ms of charging: 5 (1 - e^-1) = 3.16 V.
    CHECK(std::abs(signal(read, "V(CAP)")["value"].get<double>() - 3.16) < 0.1);
    f.ok("sim.stop");
}


TEST_CASE("automation: the HC-SR501 PIR holds its output for the delay after the movement (SN-229)")
{
    Fixture f;
    f.ok("component.add", {{"type", "vdc"}, {"at", {0, 0}}, {"parameters", {{"V", "5"}}}});
    f.ok("component.add", {{"type", "ground"}, {"attach", {{"to", "V1.-"}}}});
    f.ok("label.add", {{"name", "P5"}, {"at", "V1.+"}});
    f.ok("component.add", {{"type", "pir_hcsr501"}, {"at", {20, 0}}, {"parameters", {{"delay", "1"}}}});
    f.ok("label.add", {{"name", "P5"}, {"at", "U1.VCC"}});
    f.ok("label.add", {{"name", "PIR"}, {"at", "U1.OUT"}});
    f.ok("component.add", {{"type", "ground"}, {"attach", {{"to", "U1.GND"}}}});
    f.ok("component.add", {{"type", "resistor"}, {"at", {30, 4}}, {"parameters", {{"R", "10k"}}}});
    f.ok("label.add", {{"name", "PIR"}, {"at", "R1.1"}});
    f.ok("component.add", {{"type", "ground"}, {"attach", {{"to", "R1.2"}}}});
    REQUIRE(f.ok("circuit.check")["ok"] == true);
    const auto result = f.ok("sim.run", {{"mode", "interactive"}, {"duration", "2.5"}, {"signals", {"V(PIR)"}},
        {"events", {{{"at", "0.2"}, {"component", "U1"}, {"value", "pressed"}},
                       {{"at", "0.5"}, {"component", "U1"}, {"value", "released"}}}},
        {"points", 26}});
    REQUIRE(result.is_object());
    const auto& points = signal(result, "V(PIR)")["points"];
    auto at = [&](double t) {
        double v = 0;
        for (const auto& p : points) {
            if (p[0].get<double>() <= t + 1e-6) {
                v = p[1].get<double>();
            }
        }
        return v;
    };
    CHECK(at(0.1) < 0.2);  // nobody yet
    CHECK(at(0.4) > 3.0);  // moving
    CHECK(at(1.3) > 3.0);  // within the 1 s delay after 0.5 s
    CHECK(at(1.8) < 0.2);  // over
}

TEST_CASE("automation: sim.control turns a knob over a time and sets a chip's value; project.new takes a name (SN-240)")
{
    Fixture f;
    REQUIRE(f.ok("project.new", {{"name", "Knob demo"}})["name"] == "Knob demo");
    // 5 V across a potentiometer; the wiper is OUT.
    f.ok("component.add", {{"type", "vdc"}, {"at", {0, 0}}, {"parameters", {{"V", "5"}}}});
    f.ok("component.add", {{"type", "potentiometer"}, {"at", {10, 0}}});
    f.ok("component.add", {{"type", "ground"}, {"attach", {{"to", "V1.-"}}}});
    REQUIRE(f.ok("wire.connect", {{"from", "V1.+"}, {"to", "RV1.1"}}).is_object());
    REQUIRE(f.ok("wire.connect", {{"from", "RV1.2"}, {"to", "V1.-"}}).is_object());
    f.ok("label.add", {{"name", "OUT"}, {"at", "RV1.W"}});
    // An HC-SR04 on its own (a chip group without a board), powered from V1.
    f.ok("component.add", {{"type", "hcsr04"}, {"at", {30, 0}}});
    REQUIRE(f.ok("wire.connect", {{"from", "U1.VCC"}, {"to", "V1.+"}}).is_object());
    f.ok("component.add", {{"type", "ground"}, {"attach", {{"to", "U1.GND"}}}});

    REQUIRE(f.ok("sim.start", {{"signals", {"V(OUT)"}}}).is_object());
    REQUIRE(f.ok("sim.control", {{"component", "RV1"}, {"value", "20%"}}).is_object());
    const auto begin = std::chrono::steady_clock::now();
    const auto turned = f.ok("sim.control", {{"component", "RV1"}, {"value", "80%"}, {"over", "300m"}});
    REQUIRE(turned.is_object());
    CHECK(std::chrono::steady_clock::now() - begin >= std::chrono::milliseconds(280));
    CHECK(std::abs(turned["value"].get<double>() - 0.8) < 1e-9);

    // The sensor's distance through sim.control, as a switch would be.
    const auto set = f.ok("sim.control", {{"component", "U1"}, {"value", "150 cm"}, {"over", "100m"}});
    REQUIRE(set.is_object());
    CHECK(set["parameter"] == "distance");
    CHECK(set["value"] == "150");
    const auto parts = f.ok("circuit.inspect");
    for (const auto& c : parts["components"]) {
        if (c["designator"] == "U1") {
            CHECK(c["parameters"]["distance"] == "150");
        }
    }
    CHECK(f.ok("sim.control", {{"component", "V1"}, {"value", 1}}).is_null()); // neither a control nor a chip
    f.ok("sim.stop");
}
