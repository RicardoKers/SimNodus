// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "test_support.hpp"

#include "sim/measurements.hpp"
#include "sim/ngspice_runtime.hpp"
#include "sim/process_memory.hpp"
#include "sim/sample_ring.hpp"
#include "sim/vcd.hpp"

#include <algorithm>
#include <cmath>

using namespace simnodus::sim;

TEST_CASE("sample ring keeps the newest samples in order")
{
    SampleRing ring(3);
    CHECK(ring.snapshot().empty());
    for (int i = 0; i < 5; ++i) {
        ring.push(i * 0.1, i);
    }
    const auto samples = ring.snapshot();
    REQUIRE(samples.size() == 3);
    CHECK(samples[0].value == 2.0);
    CHECK(samples[1].value == 3.0);
    CHECK(samples[2].value == 4.0);
    CHECK(samples[2].time == 0.4);
    CHECK(ring.dropped() == 2);
    ring.clear();
    CHECK(ring.size() == 0);
    CHECK(ring.dropped() == 0);
}

TEST_CASE("sample ring returns the recent part")
{
    SampleRing ring(5);
    for (int i = 0; i < 8; ++i) {
        ring.push(i, i);
    }
    const auto recent = ring.since(5.5); // ring holds 3..7
    REQUIRE(recent.size() == 3);       // 5 (just before), 6, 7
    CHECK(recent[0].time == 5.0);
    CHECK(recent[2].time == 7.0);
    CHECK(ring.since(100).size() == 1);
    CHECK(ring.since(-1).size() == 5);
}

TEST_CASE("sample ring with zero capacity holds one sample")
{
    SampleRing ring(0);
    ring.push(1.0, 1.0);
    ring.push(2.0, 2.0);
    CHECK(ring.capacity() == 1);
    REQUIRE(ring.snapshot().size() == 1);
    CHECK(ring.snapshot()[0].value == 2.0);
}

TEST_CASE("ngspice version is parsed from engine output")
{
    CHECK(parse_ngspice_version("** ngspice-47 : Circuit level simulation program") == 47);
    CHECK(parse_ngspice_version("stdout ******\nstdout ** ngspice-42 : Circuit") == 42);
    CHECK(!parse_ngspice_version("no version here").has_value());
}

TEST_CASE("bundled ngspice is the first candidate")
{
    const auto candidates = ngspice_candidates("app");
    REQUIRE(!candidates.empty());
    CHECK(candidates.front().scripts_dir == std::filesystem::path("app") / "ngspice");
#ifdef _WIN32
    CHECK(candidates.front().library == std::filesystem::path("app") / "ngspice" / "ngspice.dll");
    CHECK(candidates.size() == 1);
#else
    CHECK(candidates.front().library == std::filesystem::path("app") / "ngspice" / "libngspice.so.0");
#endif
}

TEST_CASE("process memory query returns a resident size")
{
    const auto memory = current_process_memory();
    CHECK(memory.resident_bytes > 0);
}

TEST_CASE("measurements of a square wave")
{
    // 1 kHz, 0..5 V, 25 % duty, sampled every 1 us with 1 us edges.
    std::vector<Sample> s;
    for (int i = 0; i <= 10000; ++i) {
        const double t = i * 1e-6;
        const double phase = std::fmod(t, 1e-3);
        s.push_back({t, phase < 0.25e-3 ? 5.0 : 0.0});
    }
    const auto m = measure(s, 0, 10e-3);
    REQUIRE(m.has_value());
    CHECK(m->min == 0.0);
    CHECK(m->max == 5.0);
    CHECK(m->peak_to_peak == 5.0);
    CHECK(std::abs(m->mean - 1.25) < 0.01);
    CHECK(std::abs(m->rms - 5.0 * std::sqrt(0.25)) < 0.02);
    REQUIRE(m->frequency.has_value());
    CHECK(std::abs(*m->frequency - 1000.0) < 1.0);
    REQUIRE(m->duty.has_value());
    CHECK(std::abs(*m->duty - 0.25) < 0.01);
    CHECK(!measure(s, 1.0, 2.0).has_value());
}

TEST_CASE("measurements of a sine and a constant")
{
    std::vector<Sample> s;
    for (int i = 0; i <= 20000; ++i) {
        const double t = i * 1e-6;
        s.push_back({t, 2.0 * std::sin(2 * 3.14159265358979 * 500 * t)});
    }
    const auto m = measure(s, 0, 20e-3);
    REQUIRE(m.has_value());
    CHECK(std::abs(m->rms - 2.0 / std::sqrt(2.0)) < 1e-3);
    CHECK(std::abs(m->mean) < 1e-3);
    CHECK(std::abs(m->frequency.value_or(0) - 500.0) < 0.5);
    const auto flat = measure({{0, 3.3}, {1, 3.3}}, 0, 1);
    REQUIRE(flat.has_value());
    CHECK(!flat->frequency.has_value());
    CHECK(flat->rms == 3.3);
}

TEST_CASE("trigger finds the last crossing")
{
    const std::vector<Sample> s{{0, 0}, {1, 2}, {2, 0}, {3, 2}, {4, 0}};
    CHECK(std::abs(find_trigger(s, 1.0, true, 0, 4).value_or(-1) - 2.5) < 1e-12);
    CHECK(std::abs(find_trigger(s, 1.0, false, 0, 4).value_or(-1) - 3.5) < 1e-12);
    CHECK(std::abs(find_trigger(s, 1.0, true, 0, 2).value_or(-1) - 0.5) < 1e-12);
    CHECK(!find_trigger(s, 5.0, true, 0, 4).has_value());
    CHECK(std::abs(value_at(s, 0.5).value_or(-1) - 1.0) < 1e-12);
    CHECK(!value_at(s, 9).has_value());
}

TEST_CASE("VCD names keep letters, digits and underscores")
{
    CHECK(vcd_name("U1.PA0") == "U1_PA0");
    CHECK(vcd_name("V(OUT)") == "V_OUT");
    CHECK(vcd_name("V(A,B)") == "V_A_B");
    CHECK(vcd_name("()") == "signal");
}

TEST_CASE("VCD lists the selected signals and their value changes")
{
    const std::vector<VcdSignal> signals{
        {"U1.PC13", true, {{500, false}, {2000, true}, {3000, true}}},
        {"V(OUT)", false, {{1500, true}, {2000, false}, {2000, true}}},
        {"U1.PC13", false, {}},
    };
    const std::string expected = "$version SimNodus 0.3.0 $end\n"
                                 "$comment interactive run $end\n"
                                 "$timescale 1 ns $end\n"
                                 "$scope module simnodus $end\n"
                                 "$var wire 1 ! U1_PC13 $end\n"
                                 "$var wire 1 \" V_OUT $end\n"
                                 "$var wire 1 # U1_PC13_2 $end\n"
                                 "$upscope $end\n"
                                 "$enddefinitions $end\n"
                                 "#1000\n"
                                 "$dumpvars\n"
                                 "0!\n"
                                 "0\"\n"
                                 "0#\n"
                                 "$end\n"
                                 "#1500\n"
                                 "1\"\n"
                                 "#2000\n"
                                 "1!\n";
    // The change at 500 ns falls before the start and sets the initial level;
    // at 2000 ns V(OUT) ends where it was, so only U1.PC13 is written.
    CHECK(write_vcd(signals, 1000, "SimNodus 0.3.0", "interactive run") == expected);
}

int main(int argc, char** argv)
{
    return simnodus::test::run_all(argc > 1 ? argv[1] : "");
}

TEST_CASE("logic lane timing: period, pulse widths and duty cycle")
{
    // Low at first; high from 1 to 2 ms, 3 to 4 ms, 5 to 6 ms, 7 to 8 ms.
    std::vector<std::pair<double, bool>> changes;
    for (int k = 0; k < 4; ++k) {
        changes.push_back({(1 + 2 * k) * 1e-3, true});
        changes.push_back({(2 + 2 * k) * 1e-3, false});
    }
    const auto m = measure_logic(false, changes, 0, 10e-3);
    CHECK(m.rising == 4);
    CHECK(m.falling == 4);
    REQUIRE((m.period && m.frequency && m.high_time && m.low_time && m.duty));
    CHECK(std::abs(*m.period - 2e-3) < 1e-12);
    CHECK(std::abs(*m.frequency - 500.0) < 1e-6);
    CHECK(std::abs(*m.high_time - 1e-3) < 1e-12);
    CHECK(std::abs(*m.low_time - 1e-3) < 1e-12); // complete low pulses only (2..3, 4..5, 6..7 ms)
    CHECK(std::abs(*m.duty - 0.4) < 1e-12);
    CHECK(logic_level_at(false, changes, 1.5e-3));
    CHECK(!logic_level_at(false, changes, 2.5e-3));
    const auto part = measure_logic(false, changes, 1.5e-3, 3.5e-3); // starts high
    CHECK(part.rising == 1);
    CHECK(part.falling == 1);
    CHECK(!part.period);
    CHECK(std::abs(*part.duty - 0.5) < 1e-12);
}

TEST_CASE("thinning keeps peaks, edges and the time order")
{
    SampleRing ring(100000);
    for (int i = 0; i < 100000; ++i) {
        const double t = i * 1e-4;
        double v = (i / 500) % 2 ? 5.0 : 0.0; // 10 Hz square wave
        if (i == 33333) {
            v = 9.0; // a single-sample spike
        }
        ring.push(t, v);
    }
    const auto thin = ring.since(0.0, 0.01); // 10 s in 1000 buckets
    CHECK(thin.size() <= 4004);
    CHECK(thin.size() > 1000);
    // The same buckets in a later window: the shared part is thinned the same way.
    const auto later = ring.since(2.0, 0.01);
    const auto shared = std::find_if(thin.begin(), thin.end(), [&](const Sample& s) { return s.time >= later[1].time; });
    REQUIRE(shared != thin.end());
    CHECK(shared->time == later[1].time);
    CHECK(shared->value == later[1].value);
    bool spike = false;
    for (std::size_t i = 1; i < thin.size(); ++i) {
        CHECK(thin[i].time >= thin[i - 1].time);
        spike = spike || thin[i].value == 9.0;
    }
    CHECK(spike);
    bool initial = true;
    const auto edges = ring.transitions(0.0, 2.5, initial);
    CHECK(!initial);
    CHECK(edges.size() == 199 + 2); // 199 square-wave edges; the spike rises from 0 V and falls back
}
