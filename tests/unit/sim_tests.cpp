// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "test_support.hpp"

#include "sim/ngspice_runtime.hpp"
#include "sim/process_memory.hpp"
#include "sim/sample_ring.hpp"

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

int main(int argc, char** argv)
{
    return simnodus::test::run_all(argc > 1 ? argv[1] : "");
}
