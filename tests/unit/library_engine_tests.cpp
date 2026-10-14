// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// Component set A against the real engine (SN-111): every template and
// model must simulate and behave like the part it represents.
#include "test_support.hpp"

#include "circuit_builder.hpp"
#include "engine_fixture.hpp"
#include "netlist/netlist.hpp"

#include <algorithm>
#include <cmath>

using namespace simnodus;

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

// Generates the netlist (non-interactive), runs `analysis` and returns the
// requested vectors; empty on failure.
std::map<std::string, std::vector<double>> simulate(const test::CircuitBuilder& circuit, const std::string& analysis,
    const std::vector<std::string>& vectors)
{
    std::map<std::string, std::vector<double>> result;
    auto* session = test::engine();
    const auto n = netlist::generate_netlist(circuit.schematic, components(), {false, 0.0, {}});
    for (const auto& d : n.diagnostics) {
        if (d.severity == netlist::Severity::Error) {
            std::printf("  netlist error: %s\n", d.message.c_str());
        }
    }
    if (!n.ok() || !session) {
        return result;
    }
    const auto deck = netlist::build_deck(n, "library check", {analysis});
    const bool ok = session->load_circuit(deck) && session->command("run");
    if (ok) {
        for (const auto& v : vectors) {
            result[v] = session->vector(v);
        }
    } else {
        for (const auto& line : deck) {
            std::printf("  | %s\n", line.c_str());
        }
        for (const auto& line : session->take_output()) {
            std::printf("  > %s\n", line.c_str());
        }
    }
    session->command("destroy all");
    session->command("remcirc");
    session->take_output();
    return result;
}

double op(const test::CircuitBuilder& circuit, const std::string& vector)
{
    const auto r = simulate(circuit, ".op", {vector});
    const auto it = r.find(vector);
    return it == r.end() || it->second.empty() ? std::nan("") : it->second.front();
}

bool close_to(double value, double expected, double tolerance)
{
    const bool ok = std::abs(value - expected) <= tolerance;
    if (!ok) {
        std::printf("  got %.6g, expected %.6g +- %.3g\n", value, expected, tolerance);
    }
    return ok;
}

double switched(const std::string& type, const std::map<std::string, std::string>& pins,
    const std::map<std::string, std::string>& parameters, const std::string& measured)
{
    test::CircuitBuilder b(components());
    b.add("vdc", {{"+", "VCC"}, {"-", "0"}}, {{"V", "5"}});
    b.add(type, pins, parameters);
    b.add("resistor", {{"1", "OUT"}, {"2", "0"}});
    b.add("resistor", {{"1", "B"}, {"2", "0"}});
    return op(b, measured);
}

} // namespace

TEST_CASE("library: resistor divider, battery and current source")
{
    REQUIRE(test::engine() != nullptr);
    {
        test::CircuitBuilder b(components());
        b.add("vdc", {{"+", "IN"}, {"-", "0"}}, {{"V", "5"}});
        b.add("resistor", {{"1", "IN"}, {"2", "MID"}});
        b.add("resistor", {{"1", "MID"}, {"2", "0"}});
        CHECK(close_to(op(b, "mid"), 2.5, 1e-9));
    }
    {
        test::CircuitBuilder b(components());
        b.add("battery", {{"+", "P"}, {"-", "0"}}, {{"V", "9"}, {"Rint", "0.1"}});
        b.add("resistor", {{"1", "P"}, {"2", "0"}}, {{"R", "9.9"}});
        CHECK(close_to(op(b, "p"), 8.91, 1e-9));
    }
    {
        test::CircuitBuilder b(components());
        b.add("idc", {{"+", "P"}, {"-", "0"}}, {{"I", "1m"}});
        b.add("resistor", {{"1", "P"}, {"2", "0"}});
        CHECK(close_to(op(b, "p"), 1.0, 1e-9)); // current flows out of +
    }
}

TEST_CASE("library: potentiometer, switches and buttons")
{
    REQUIRE(test::engine() != nullptr);
    {
        test::CircuitBuilder b(components());
        b.add("vdc", {{"+", "TOP"}, {"-", "0"}}, {{"V", "10"}});
        b.add("potentiometer", {{"1", "TOP"}, {"W", "W"}, {"2", "0"}}, {{"position", "25"}});
        CHECK(close_to(op(b, "w"), 7.5, 1e-3));
    }
    CHECK(close_to(switched("spst", {{"1", "VCC"}, {"2", "OUT"}}, {{"closed", "closed"}}, "out"), 5.0, 1e-3));
    CHECK(close_to(switched("spst", {{"1", "VCC"}, {"2", "OUT"}}, {{"closed", "open"}}, "out"), 0.0, 1e-3));
    CHECK(close_to(switched("button_no", {{"1", "VCC"}, {"2", "OUT"}}, {}, "out"), 0.0, 1e-3));
    CHECK(close_to(switched("button_nc", {{"1", "VCC"}, {"2", "OUT"}}, {}, "out"), 5.0, 1e-3));
    CHECK(close_to(switched("spdt", {{"C", "VCC"}, {"A", "OUT"}, {"B", "B"}}, {{"closed", "open"}}, "out"), 5.0, 1e-3));
    CHECK(close_to(switched("spdt", {{"C", "VCC"}, {"A", "OUT"}, {"B", "B"}}, {{"closed", "open"}}, "b"), 0.0, 1e-3));
    CHECK(close_to(switched("spdt", {{"C", "VCC"}, {"A", "OUT"}, {"B", "B"}}, {{"closed", "closed"}}, "b"), 5.0, 1e-3));
}

TEST_CASE("library: semiconductors behave like their parts")
{
    REQUIRE(test::engine() != nullptr);
    {
        test::CircuitBuilder b(components());
        b.add("vdc", {{"+", "VCC"}, {"-", "0"}}, {{"V", "5"}});
        b.add("resistor", {{"1", "VCC"}, {"2", "A"}});
        b.add("diode", {{"A", "A"}, {"K", "0"}});
        CHECK(close_to(op(b, "a"), 0.65, 0.1));
    }
    const std::map<std::string, std::pair<double, double>> led_vf{
        {"red", {1.75, 2.05}}, {"yellow", {1.85, 2.15}}, {"green", {1.95, 2.3}}, {"blue", {2.7, 3.3}}};
    for (const auto& [color, range] : led_vf) {
        test::CircuitBuilder b(components());
        b.add("vdc", {{"+", "VCC"}, {"-", "0"}}, {{"V", "5"}});
        b.add("resistor", {{"1", "VCC"}, {"2", "A"}}, {{"R", "220"}});
        b.add("led", {{"A", "A"}, {"K", "0"}}, {{"color", color}});
        const double vf = op(b, "a");
        std::printf("  %s LED: %.3f V at %.1f mA\n", color.c_str(), vf, (5 - vf) / 220 * 1e3);
        CHECK(vf > range.first && vf < range.second);
    }
    {
        test::CircuitBuilder b(components());
        b.add("vdc", {{"+", "VCC"}, {"-", "0"}}, {{"V", "5"}});
        b.add("resistor", {{"1", "VCC"}, {"2", "B"}}, {{"R", "10k"}});
        b.add("resistor", {{"1", "VCC"}, {"2", "C"}}, {{"R", "1k"}});
        b.add("npn", {{"B", "B"}, {"C", "C"}, {"E", "0"}});
        CHECK(op(b, "c") < 0.3); // saturated
    }
    {
        test::CircuitBuilder b(components());
        b.add("vdc", {{"+", "VCC"}, {"-", "0"}}, {{"V", "5"}});
        b.add("resistor", {{"1", "VCC"}, {"2", "D"}}, {{"R", "1k"}});
        b.add("nmos", {{"G", "VCC"}, {"D", "D"}, {"S", "0"}});
        CHECK(op(b, "d") < 0.5); // switched on
    }
}

TEST_CASE("library: instruments read voltage and current")
{
    REQUIRE(test::engine() != nullptr);
    test::CircuitBuilder b(components());
    b.add("vdc", {{"+", "VCC"}, {"-", "0"}}, {{"V", "5"}});
    b.add("ammeter", {{"+", "VCC"}, {"-", "A"}});
    b.add("resistor", {{"1", "A"}, {"2", "0"}});
    b.add("voltmeter", {{"+", "A"}, {"-", "0"}});
    CHECK(close_to(op(b, "v_am1#branch"), 5e-3, 1e-9));
    CHECK(close_to(op(b, "a"), 5.0, 1e-6));
}

TEST_CASE("library: a current probe measures the current into a pin")
{
    REQUIRE(test::engine() != nullptr);
    test::CircuitBuilder b(components());
    b.add("vdc", {{"+", "IN"}, {"-", "0"}}, {{"V", "5"}});
    const auto r = b.add("resistor", {{"1", "IN"}, {"2", "0"}}, {{"R", "1k"}});
    netlist::NetlistOptions options;
    options.interactive = false;
    options.current_probes = {{r, "1"}, {r, "2"}};
    const auto n = netlist::generate_netlist(b.schematic, components(), options);
    REQUIRE(n.ok());
    auto* session = test::engine();
    REQUIRE(session->load_circuit(netlist::build_deck(n, "probe", {".op"})) && session->command("run"));
    const auto into1 = session->vector(n.pin_current.at({r, "1"}));
    const auto into2 = session->vector(n.pin_current.at({r, "2"}));
    REQUIRE(!into1.empty() && !into2.empty());
    CHECK(close_to(into1[0], 5e-3, 1e-9));  // flows into pin 1
    CHECK(close_to(into2[0], -5e-3, 1e-9)); // and out of pin 2
    session->command("destroy all");
    session->command("remcirc");
}

TEST_CASE("library: reactive parts and generators in a transient")
{
    REQUIRE(test::engine() != nullptr);
    {
        test::CircuitBuilder b(components());
        b.add("vdc", {{"+", "VCC"}, {"-", "0"}}, {{"V", "5"}});
        b.add("resistor", {{"1", "VCC"}, {"2", "A"}}, {{"R", "100"}});
        b.add("inductor", {{"1", "A"}, {"2", "0"}}, {{"L", "10m"}});
        b.add("resistor", {{"1", "VCC"}, {"2", "B"}}, {{"R", "1k"}});
        b.add("capacitor", {{"1", "B"}, {"2", "0"}}, {{"C", "1u"}});
        const auto r = simulate(b, ".tran 10u 5m 0 10u uic", {"l_l1#branch", "b"});
        REQUIRE(r.count("b") && !r.at("b").empty());
        CHECK(close_to(r.at("l_l1#branch").back(), 0.05, 1e-3)); // 50 tau: 5 V / 100 Ohm
        CHECK(close_to(r.at("b").back(), 5 * (1 - std::exp(-5.0)), 0.02));
    }
    {
        test::CircuitBuilder b(components());
        b.add("vpulse", {{"+", "P"}, {"-", "0"}}, {{"f", "1k"}, {"Vhigh", "3.3"}});
        b.add("resistor", {{"1", "P"}, {"2", "0"}});
        b.add("vsine", {{"+", "S"}, {"-", "0"}}, {{"amplitude", "2"}, {"f", "500"}});
        b.add("resistor", {{"1", "S"}, {"2", "0"}});
        const auto r = simulate(b, ".tran 1u 4m 0 1u", {"p", "s"});
        REQUIRE(r.count("p") && !r.at("p").empty());
        const auto [pmin, pmax] = std::minmax_element(r.at("p").begin(), r.at("p").end());
        const auto [smin, smax] = std::minmax_element(r.at("s").begin(), r.at("s").end());
        CHECK(close_to(*pmin, 0.0, 1e-9));
        CHECK(close_to(*pmax, 3.3, 1e-9));
        CHECK(close_to(*smax, 2.0, 1e-3));
        CHECK(close_to(*smin, -2.0, 1e-3));
    }
}
