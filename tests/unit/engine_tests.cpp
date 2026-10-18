// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// Runs against a real ngspice library: engine_tests <library> <scripts-dir>.
// ngspice is process-global, so one session serves every case.
#include "test_support.hpp"

#include "engine_fixture.hpp"
#include "sim/engine_host.hpp"
#include "sim/ngspice_runtime.hpp"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <memory>

using namespace simnodus::sim;

namespace {

NgspiceLocation location;
NgspiceSession* session = nullptr;

const std::vector<std::string> rc_netlist = {
    "* RC step, tau = 1 ms",
    "V1 in 0 DC 3.3",
    "R1 in out 1k",
    "C1 out 0 1u IC=0",
    ".options reltol=1e-6 abstol=1e-12 vntol=1e-9",
    ".tran 1u 5m 0 1u uic",
    ".end",
};

} // namespace

TEST_CASE("session opens and reports the engine version")
{
    REQUIRE(simnodus::test::engine() != nullptr);
    REQUIRE(session->version().has_value());
    std::printf("  ngspice-%d from %s\n", *session->version(), location.library.string().c_str());
    CHECK(*session->version() >= 42);

    std::string second_error;
    CHECK(NgspiceSession::open(location, second_error) == nullptr);
    CHECK(!second_error.empty());
}

TEST_CASE("foreground RC transient matches the analytical response")
{
    REQUIRE(simnodus::test::engine() != nullptr);
    std::size_t callbacks = 0;
    session->set_data_handler([&](const AcceptedPoint&) { ++callbacks; });
    REQUIRE(session->load_circuit(rc_netlist));
    REQUIRE(session->command("run"));
    const auto time = session->vector("time");
    const auto out = session->vector("out");
    REQUIRE(!time.empty());
    REQUIRE(time.size() == out.size());
    double worst = 0.0;
    for (std::size_t i = 0; i < time.size(); ++i) {
        worst = std::max(worst, std::abs(out[i] - 3.3 * (1.0 - std::exp(-time[i] / 1e-3))));
    }
    std::printf("  %zu points, max error %.3g V\n", time.size(), worst);
    CHECK(worst < 0.0165); // E-01 gate: 0.5 % of full scale
    CHECK(std::abs(time.back() - 5e-3) < 1e-12);
    CHECK(callbacks == time.size());
    session->set_data_handler(nullptr);
    session->command("destroy all");
    session->command("remcirc");
}

TEST_CASE("external source drives a background run")
{
    REQUIRE(simnodus::test::engine() != nullptr);
    std::atomic<int> queries{0};
    std::atomic<int> points{0};
    session->set_source_handler([&](const std::string&, double t) {
        ++queries;
        return t >= 1e-3 ? 3.3 : 0.0;
    });
    session->set_data_handler([&](const AcceptedPoint&) { ++points; });
    REQUIRE(session->load_circuit({
        "* externally driven RC",
        "V1 in 0 DC 0 external",
        "R1 in out 1k",
        "C1 out 0 1u IC=0",
        ".tran 1u 3m 0 1u uic",
        ".end",
    }));
    REQUIRE(session->start_background());
    REQUIRE(session->wait_background(std::chrono::seconds(30)));
    const auto time = session->vector("time");
    const auto out = session->vector("out");
    REQUIRE(!out.empty());
    // At 3 ms the capacitor has charged for 2 ms: 3.3 * (1 - e^-2).
    CHECK(std::abs(out.back() - 3.3 * (1.0 - std::exp(-2.0))) < 0.0165);
    CHECK(queries > 0);
    CHECK(points == static_cast<int>(time.size()));
    session->set_source_handler(nullptr);
    session->set_data_handler(nullptr);
    session->command("destroy all");
    session->command("remcirc");
}

TEST_CASE("an invalid netlist is reported")
{
    REQUIRE(simnodus::test::engine() != nullptr);
    CHECK(!session->load_circuit({"* missing diode model", "V1 a 0 DC 1", "D1 a 0 nosuchmodel", ".op", ".end"}));
    session->take_output();
    session->command("remcirc");
}

// The application's engine host loads ngspice on first use; test files
// register in any order, and the simulation session shares the same engine.
simnodus::sim::NgspiceSession* simnodus::test::engine()
{
    static bool attempted = false;
    if (!session && !attempted) {
        attempted = true;
        std::string error;
        session = simnodus::sim::EngineHost::instance().acquire(error);
        if (!session) {
            std::printf("  cannot open ngspice: %s\n", error.c_str());
        }
    }
    return session;
}

// The long soak cases run only with a fourth argument "soak".
bool test_soak = false;
// Segment length of the soak cases (seconds); a fifth argument changes it.
double test_soak_segment = 1.0;

int main(int argc, char** argv)
{
    if (argc < 3) {
        std::printf("usage: engine_tests <ngspice-library> <scripts-dir> [filter] [soak [segment-seconds]]\n");
        return 2;
    }
    test_soak = argc > 4 && std::string(argv[4]) == "soak";
    test_soak_segment = argc > 5 ? std::atof(argv[5]) : 1.0;
    location = {argv[1], argv[2]};
    simnodus::sim::EngineHost::instance().set_location(location);
    return simnodus::test::run_all(argc > 3 ? argv[3] : "");
}
