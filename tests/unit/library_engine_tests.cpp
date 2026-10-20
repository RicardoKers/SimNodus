// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// Component set A against the real engine (SN-111): every template and
// model must simulate and behave like the part it represents.
#include "test_support.hpp"

#include "circuit_builder.hpp"
#include "engine_fixture.hpp"
#include "editing/operations.hpp"
#include "netlist/netlist.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>

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

TEST_CASE("library: adjustable and controlled sources (SN-209)")
{
    REQUIRE(test::engine() != nullptr);
    {
        test::CircuitBuilder b(components());
        b.add("vadj", {{"+", "P"}, {"-", "0"}}, {{"Vmin", "1"}, {"Vmax", "9"}, {"position", "25"}});
        b.add("resistor", {{"1", "P"}, {"2", "0"}});
        CHECK(close_to(op(b, "p"), 3.0, 1e-6));
    }
    {
        test::CircuitBuilder b(components());
        b.add("iadj", {{"+", "P"}, {"-", "0"}}, {{"Imin", "0"}, {"Imax", "10m"}, {"position", "50"}});
        b.add("resistor", {{"1", "P"}, {"2", "0"}});
        CHECK(close_to(op(b, "p"), 5.0, 1e-6)); // 5 mA out of + into 1 kOhm
    }
    // Voltage control: 1 V (VCVS) or 2 V (VCCS) between C+ and C-, output into 1 kOhm.
    {
        test::CircuitBuilder b(components());
        b.add("vdc", {{"+", "C"}, {"-", "0"}}, {{"V", "1"}});
        b.add("vcvs", {{"C+", "C"}, {"C-", "0"}, {"+", "OUT"}, {"-", "0"}}, {{"gain", "10"}});
        b.add("resistor", {{"1", "OUT"}, {"2", "0"}});
        CHECK(close_to(op(b, "out"), 10.0, 1e-6));
    }
    {
        test::CircuitBuilder b(components());
        b.add("vdc", {{"+", "C"}, {"-", "0"}}, {{"V", "2"}});
        b.add("vccs", {{"C+", "C"}, {"C-", "0"}, {"+", "OUT"}, {"-", "0"}}, {{"gm", "1m"}});
        b.add("resistor", {{"1", "OUT"}, {"2", "0"}});
        CHECK(close_to(op(b, "out"), 2.0, 1e-6));
    }
    // Current control: 1 mA flows from C+ to C- (1 V across 1 kOhm).
    auto current_controlled = [](const std::string& type, const std::string& gain, const std::string& value,
                                  const std::string& load) {
        test::CircuitBuilder b(components());
        b.add("vdc", {{"+", "A"}, {"-", "0"}}, {{"V", "1"}});
        b.add(type, {{"C+", "A"}, {"C-", "B"}, {"+", "OUT"}, {"-", "0"}}, {{gain, value}});
        b.add("resistor", {{"1", "B"}, {"2", "0"}});
        b.add("resistor", {{"1", "OUT"}, {"2", "0"}}, {{"R", load}});
        return op(b, "out");
    };
    CHECK(close_to(current_controlled("cccs", "gain", "100", "10"), 1.0, 1e-6)); // 100 mA into 10 Ohm
    CHECK(close_to(current_controlled("ccvs", "rm", "2k", "1k"), 2.0, 1e-6));
}

TEST_CASE("library: A/D and D/A converters (SN-211)")
{
    REQUIRE(test::engine() != nullptr);
    // 1.3 V of 5 V in 8 bits: code floor(256 x 0.26) = 66 = 0100 0010.
    {
        test::CircuitBuilder b(components());
        b.add("vdc", {{"+", "IN"}, {"-", "0"}}, {{"V", "1.3"}});
        b.add("vdc", {{"+", "REF"}, {"-", "0"}}, {{"V", "5"}});
        std::map<std::string, std::string> pins{{"VIN", "IN"}, {"VREF", "REF"}};
        for (int k = 0; k < 8; ++k) {
            pins["D" + std::to_string(k)] = "D" + std::to_string(k);
        }
        b.add("adc_8", pins);
        const auto r = simulate(b, ".op", {"d0", "d1", "d6", "d7"});
        REQUIRE(r.size() == 4);
        CHECK(close_to(r.at("d0").front(), 0.0, 1e-3));
        CHECK(close_to(r.at("d1").front(), 5.0, 1e-3));
        CHECK(close_to(r.at("d6").front(), 5.0, 1e-3));
        CHECK(close_to(r.at("d7").front(), 0.0, 1e-3));
    }
    // Round trip at each width: OUT = VREF x code / 2^N, within one step below VIN.
    for (const int bits : {4, 8, 10, 12}) {
        test::CircuitBuilder b(components());
        b.add("vdc", {{"+", "IN"}, {"-", "0"}}, {{"V", "2.2"}});
        b.add("vdc", {{"+", "REF"}, {"-", "0"}}, {{"V", "3.3"}});
        std::map<std::string, std::string> adc{{"VIN", "IN"}, {"VREF", "REF"}};
        std::map<std::string, std::string> dac{{"VREF", "REF"}, {"OUT", "OUT"}};
        for (int k = 0; k < bits; ++k) {
            adc["D" + std::to_string(k)] = "B" + std::to_string(k);
            dac["D" + std::to_string(k)] = "B" + std::to_string(k);
        }
        b.add("adc_" + std::to_string(bits), adc, {{"VDD", "3.3"}});
        b.add("dac_" + std::to_string(bits), dac, {{"VDD", "3.3"}});
        b.add("voltmeter", {{"+", "OUT"}, {"-", "0"}});
        const double step = 3.3 / (1 << bits);
        const double expected = std::floor(2.2 / step) * step;
        CHECK(close_to(op(b, "out"), expected, 1e-4));
    }
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
    CHECK(close_to(op(b, "v_am1#branch"), 5e-3 + 5.0 / 1e9, 1e-10)); // includes the voltmeter's 1 GOhm
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

TEST_CASE("library: component set B behaves like its parts")
{
    REQUIRE(test::engine() != nullptr);
    auto supply = [](test::CircuitBuilder& b, const char* net, const char* volts) {
        b.add("vdc", {{"+", net}, {"-", "0"}}, {{"V", volts}});
    };
    { // PNP high-side switch: base pulled low turns it on
        test::CircuitBuilder b(components());
        supply(b, "VCC", "5");
        b.add("pnp", {{"E", "VCC"}, {"B", "B"}, {"C", "C"}});
        b.add("resistor", {{"1", "B"}, {"2", "0"}}, {{"R", "10k"}});
        b.add("resistor", {{"1", "C"}, {"2", "0"}}, {{"R", "1k"}});
        CHECK(op(b, "c") > 4.7);
    }
    for (const char* gate : {"0", "5"}) { // P-channel MOSFET: on with the gate low
        test::CircuitBuilder b(components());
        supply(b, "VCC", "5");
        supply(b, "G", gate);
        b.add("pmos", {{"S", "VCC"}, {"G", "G"}, {"D", "D"}});
        b.add("resistor", {{"1", "D"}, {"2", "0"}}, {{"R", "1k"}});
        const double d = op(b, "d");
        CHECK(gate[0] == '0' ? d > 4.5 : d < 0.1);
    }
    { // Zener regulator
        test::CircuitBuilder b(components());
        supply(b, "VIN", "12");
        b.add("resistor", {{"1", "VIN"}, {"2", "K"}}, {{"R", "1k"}});
        b.add("zener", {{"K", "K"}, {"A", "0"}}, {{"Vz", "5.1"}});
        CHECK(close_to(op(b, "k"), 5.1, 0.25));
    }
    for (const char* in : {"1", "3"}) { // op-amp: non-inverting gain 2 on a 5 V supply, clipped at the rail
        test::CircuitBuilder b(components());
        supply(b, "VCC", "5");
        supply(b, "IN", in);
        b.add("opamp", {{"IN+", "IN"}, {"IN-", "FB"}, {"OUT", "OUT"}, {"V+", "VCC"}, {"V-", "0"}});
        b.add("resistor", {{"1", "OUT"}, {"2", "FB"}}, {{"R", "10k"}});
        b.add("resistor", {{"1", "FB"}, {"2", "0"}}, {{"R", "10k"}});
        CHECK(close_to(op(b, "out"), in[0] == '1' ? 2.0 : 4.9, 0.02));
    }
    { // lamp: 6 V / 100 mA is 60 ohm
        test::CircuitBuilder b(components());
        supply(b, "VCC", "12");
        b.add("resistor", {{"1", "VCC"}, {"2", "MID"}}, {{"R", "60"}});
        b.add("lamp", {{"1", "MID"}, {"2", "0"}}, {{"V", "6"}, {"I", "100m"}});
        CHECK(close_to(op(b, "mid"), 6.0, 0.01));
    }
    for (const char* coil : {"5", "0"}) { // relay: an energized coil moves COM from NC to NO
        test::CircuitBuilder b(components());
        supply(b, "COIL", coil);
        supply(b, "SW", "3");
        b.add("relay", {{"A", "COIL"}, {"B", "0"}, {"COM", "SW"}, {"NO", "NO"}, {"NC", "NC"}}, {{"Vcoil", "5"}});
        b.add("resistor", {{"1", "NO"}, {"2", "0"}}, {{"R", "1k"}});
        b.add("resistor", {{"1", "NC"}, {"2", "0"}}, {{"R", "1k"}});
        const auto r = simulate(b, ".op", {"no", "nc"});
        REQUIRE(r.count("no") && r.count("nc"));
        const bool on = coil[0] == '5';
        CHECK(close_to(r.at("no").front(), on ? 3.0 : 0.0, 0.01));
        CHECK(close_to(r.at("nc").front(), on ? 0.0 : 3.0, 0.01));
    }
    { // DC motor at its rated voltage: no-load speed less the friction's share
        test::CircuitBuilder b(components());
        supply(b, "VM", "6");
        b.add("dcmotor", {{"+", "VM"}, {"-", "0"}}, {{"V", "6"}, {"rpm", "6000"}, {"Istall", "2"}, {"Inoload", "100m"}});
        CHECK(close_to(op(b, "w_m1") * 9.5492966, 6000 * (1 - 0.1 / 2), 60));
    }
    for (const char* vdd : {"5", "3.3"}) { // logic gates: truth tables, output loaded by 10 kohm
        const double high = std::atof(vdd);
        const std::vector<std::pair<const char*, int>> gates{
            {"and_gate", 0b1000}, {"nand_gate", 0b0111}, {"or_gate", 0b1110}, {"nor_gate", 0b0001}, {"xor_gate", 0b0110}};
        for (const auto& [gate, table] : gates) {
            for (int inputs = 0; inputs < 4; ++inputs) {
                test::CircuitBuilder b(components());
                supply(b, "A", inputs & 1 ? vdd : "0");
                supply(b, "B", inputs & 2 ? vdd : "0");
                b.add(gate, {{"A", "A"}, {"B", "B"}, {"Y", "Y"}}, {{"VDD", vdd}});
                b.add("resistor", {{"1", "Y"}, {"2", "0"}}, {{"R", "10k"}});
                const double expected = (table >> inputs) & 1 ? high : 0.0;
                if (!close_to(op(b, "y"), expected, 0.05)) {
                    std::printf("  %s at VDD %s, inputs %d\n", gate, vdd, inputs);
                    CHECK(false);
                }
            }
        }
        for (const bool in : {false, true}) {
            test::CircuitBuilder b(components());
            supply(b, "A", in ? vdd : "0");
            b.add("not_gate", {{"A", "A"}, {"Y", "Y"}}, {{"VDD", vdd}});
            CHECK(close_to(op(b, "y"), in ? 0.0 : high, 0.01));
        }
    }
    { // model variants: a Schottky drops less than a silicon diode; a user model is used
        auto forward = [&](const char* model, const std::string& card) {
            test::CircuitBuilder b(components());
            supply(b, "VCC", "5");
            b.add("resistor", {{"1", "VCC"}, {"2", "A"}}, {{"R", "1k"}});
            const auto d = b.add("diode", {{"A", "A"}, {"K", "0"}}, {{"model", model}});
            if (!card.empty()) {
                editing::set_model(b.schematic, d, "model", model, card).apply(b.schematic);
            }
            return op(b, "a");
        };
        const double silicon = forward("1N4148", "");
        const double schottky = forward("1N5819", "");
        const double user = forward("MYD", ".model MYD D(IS=1e-6 N=1)");
        std::printf("  forward at 4 mA: 1N4148 %.3f V, 1N5819 %.3f V, user model %.3f V\n", silicon, schottky, user);
        CHECK(silicon > 0.6 && silicon < 0.8);
        CHECK(schottky > 0.15 && schottky < silicon - 0.2);
        CHECK(user > 0.1 && user < 0.25);
        // A value that is not a number used to stop ngspice for the rest of the
        // session; it is now left out, and the next circuits still run.
        const double broken = forward("BADD", ".model BADD D(IS=abc N=1.8 mfg=Test)");
        const double after = forward("1N4148", "");
        std::printf("  model with IS=abc: %.3f V; next circuit: %.3f V\n", broken, after);
        CHECK(broken > 1.1 && broken < 1.4); // the default IS with N=1.8
        CHECK(std::abs(after - silicon) < 1e-6);
        // MOSFETs switched on with 10 V at the gate: on-resistance near the datasheet's.
        auto on_resistance = [&](const char* model, const char* load, double amps) {
            test::CircuitBuilder b(components());
            supply(b, "VCC", "12");
            supply(b, "G", "10");
            b.add("resistor", {{"1", "VCC"}, {"2", "D"}}, {{"R", load}});
            b.add("nmos", {{"D", "D"}, {"G", "G"}, {"S", "0"}}, {{"model", model}});
            return op(b, "d") / amps;
        };
        const double irf540n = on_resistance("IRF540N", "12", 1.0);
        const double n2n7000 = on_resistance("2N7000", "120", 0.1);
        std::printf("  on-resistance at 10 V: IRF540N %.3f ohm, 2N7000 %.2f ohm\n", irf540n, n2n7000);
        CHECK(irf540n > 0.02 && irf540n < 0.08);
        CHECK(n2n7000 > 1.0 && n2n7000 < 3.0);
    }
    { // vendor subcircuits: an op-amp macro model with POLY, a Darlington as an NPN
        const std::string opamp = "* SNOPAMP: op-amp macro model written for the tests (MIT)\n"
    ".SUBCKT SN/OPAMP 1 2 3 4 5\n"
    "* connections: in+ in- v+ v- out\n"
    "RIN 1 2 1MEG\n"
    "RS 3 4 1MEG\n"
    "EGAIN 6 0 POLY(1) 1 2 0 1E5\n"
    "RP 6 7 1K\n"
    "CP 7 0 1.59U\n"
    "EBUF 8 0 7 0 1\n"
    "RO 8 5 50\n"
    ".ENDS\n";
        test::CircuitBuilder b(components());
        supply(b, "VCC", "5");
        supply(b, "IN", "1");
        const auto u = b.add("opamp", {{"IN+", "IN"}, {"IN-", "FB"}, {"OUT", "OUT"}, {"V+", "VCC"}, {"V-", "0"}});
        b.add("resistor", {{"1", "OUT"}, {"2", "FB"}}, {{"R", "10k"}});
        b.add("resistor", {{"1", "FB"}, {"2", "0"}}, {{"R", "10k"}});
        editing::set_model(b.schematic, u, "model", "SN/OPAMP", opamp).apply(b.schematic);
        const double out = op(b, "out");
        std::printf("  subcircuit op-amp, gain 2 at 1 V: %.4f V (code models %s)\n", out,
            test::engine()->has_code_models() ? "loaded" : "missing");
        // POLY sources need spice2poly.cm: packaged on Windows, from the ngspice
        // package on Linux. Without it the run must fail, never give a value.
        if (test::engine()->has_code_models()) {
            CHECK(close_to(out, 2.0, 0.01));
        } else {
            std::printf("  skipped the POLY check: spice2poly.cm not found\n");
            CHECK(std::isnan(out));
        }
        test::CircuitBuilder d(components());
        supply(d, "VCC", "12");
        supply(d, "G", "5");
        d.add("resistor", {{"1", "VCC"}, {"2", "C"}}, {{"R", "12"}});
        d.add("resistor", {{"1", "G"}, {"2", "B"}}, {{"R", "1k"}});
        const auto q = d.add("npn", {{"C", "C"}, {"B", "B"}, {"E", "0"}});
        editing::set_model(d.schematic, q, "model", "SNDARL", ".SUBCKT SNDARL 1 2 3\n"
    "* TERMINALS: C B E\n"
    "Q1 1 2 4 QD\n"
    "Q2 1 4 3 QD\n"
    "R1 2 4 10K\n"
    "R2 4 3 150\n"
    ".ENDS\n"
    ".MODEL QD NPN(BF=100 IS=1E-14)\n").apply(d.schematic);
        const double vce = op(d, "c");
        std::printf("  Darlington subcircuit at 1 A: Vce %.3f V\n", vce);
        CHECK(vce > 0.6 && vce < 1.5); // a Darlington does not saturate below one diode drop
    }
    { // teaching models: a fixed diode drop, simple transistors with beta and Vbe
        auto drop = [&](const char* vf) {
            test::CircuitBuilder b(components());
            supply(b, "VCC", "5");
            b.add("resistor", {{"1", "VCC"}, {"2", "A"}}, {{"R", "1k"}});
            b.add("ideal_diode", {{"A", "A"}, {"K", "0"}}, {{"Vf", vf}});
            return op(b, "a");
        };
        CHECK(close_to(drop("0.7"), 0.7, 0.002));
        CHECK(close_to(drop("0.3"), 0.3, 0.002));
        auto npn = [&](const char* rb) {
            test::CircuitBuilder b(components());
            supply(b, "VCC", "12");
            supply(b, "IN", "5");
            b.add("resistor", {{"1", "IN"}, {"2", "B"}}, {{"R", rb}});
            b.add("resistor", {{"1", "VCC"}, {"2", "C"}}, {{"R", "1k"}});
            b.add("simple_npn", {{"C", "C"}, {"B", "B"}, {"E", "0"}}, {{"beta", "100"}});
            return std::make_pair(op(b, "b"), op(b, "c"));
        };
        const auto active = npn("100k"); // Ib = 43 uA, Ic = 4.3 mA
        const auto saturated = npn("10k"); // beta x Ib would be 43 mA, the load allows 12 mA
        std::printf("  simple NPN: Vbe %.3f V, Vc %.3f V active; Vc %.3f V saturated\n", active.first, active.second,
            saturated.second);
        CHECK(close_to(active.first, 0.7, 0.002));
        CHECK(close_to(active.second, 12.0 - 4.3, 0.05));
        CHECK(saturated.second > 0.17 && saturated.second < 0.2); // at Vce(sat)
        test::CircuitBuilder p(components());
        supply(p, "VCC", "12");
        p.add("resistor", {{"1", "B"}, {"2", "0"}}, {{"R", "100k"}}); // Ib = 113 uA
        p.add("resistor", {{"1", "C"}, {"2", "0"}}, {{"R", "470"}});
        p.add("simple_pnp", {{"C", "C"}, {"B", "B"}, {"E", "VCC"}}, {{"beta", "100"}});
        CHECK(close_to(op(p, "c"), 0.0113 * 470, 0.05));
        // Schottky and germanium parts use their own kinds of models by default.
        auto forward = [&](const char* type) {
            test::CircuitBuilder b(components());
            supply(b, "VCC", "5");
            b.add("resistor", {{"1", "VCC"}, {"2", "A"}}, {{"R", "4.7k"}}); // about 1 mA
            b.add(type, {{"A", "A"}, {"K", "0"}});
            return op(b, "a");
        };
        const double schottky = forward("schottky");
        const double germanium = forward("germanium");
        std::printf("  at 1 mA: Schottky (1N5819) %.3f V, germanium (1N34A) %.3f V\n", schottky, germanium);
        CHECK(schottky > 0.1 && schottky < 0.3);
        CHECK(germanium > 0.2 && germanium < 0.35);
    }
    { // three inverters in a ring oscillate through the gate delay
        test::CircuitBuilder b(components());
        b.add("not_gate", {{"A", "N1"}, {"Y", "N2"}});
        b.add("not_gate", {{"A", "N2"}, {"Y", "N3"}});
        b.add("not_gate", {{"A", "N3"}, {"Y", "N1"}});
        b.add("resistor", {{"1", "N1"}, {"2", "0"}}, {{"R", "1meg"}}); // the circuit's ground reference
        const auto r = simulate(b, ".tran 1n 1u 0 1n uic", {"time", "n1"});
        REQUIRE(r.count("n1") && r.count("time") && !r.at("n1").empty());
        int edges = 0;
        bool high = r.at("n1").front() > 2.5;
        for (const double v : r.at("n1")) {
            if ((v > 2.5) != high) {
                high = !high;
                ++edges;
            }
        }
        CHECK(edges > 10); // about 15 MHz from 3 x 10 ns stages
    }
    { // SN-199: latches, flip-flops, counter and shift register (behavioral, ADR 0099)
        // The clock starts high and rises at 0.5 ms, 1.5 ms, ...: after edge n, sample at n ms.
        auto clocked = [&](test::CircuitBuilder& b) {
            b.add("vpulse", {{"+", "CLK"}, {"-", "0"}}, {{"Vlow", "5"}, {"Vhigh", "0"}, {"f", "1k"}, {"rise", "1u"}});
        };
        auto at = [](const std::map<std::string, std::vector<double>>& r, const char* v, double t) {
            const auto& time = r.at("time");
            for (std::size_t i = 0; i < time.size(); ++i) {
                if (time[i] >= t) {
                    return r.at(v)[i];
                }
            }
            return std::nan("");
        };
        auto bit = [](double volts) { return volts > 2.5 ? 1 : 0; };
        {
            test::CircuitBuilder b(components());
            clocked(b);
            supply(b, "HIGH", "5");
            b.add("t_flipflop", {{"T", "HIGH"}, {"CLK", "CLK"}, {"Q", "T"}, {"QN", "TN"}});
            b.add("d_flipflop", {{"D", "HIGH"}, {"CLK", "CLK"}, {"Q", "D"}});
            b.add("jk_flipflop", {{"J", "HIGH"}, {"K", "HIGH"}, {"CLK", "CLK"}, {"Q", "JK"}});
            supply(b, "LOW", "0");
            b.add("d_latch", {{"D", "HIGH"}, {"EN", "CLK"}, {"Q", "L"}});
            b.add("d_latch", {{"D", "HIGH"}, {"EN", "LOW"}, {"Q", "L0"}});
            b.add("counter4", {{"CLK", "CLK"}, {"Q0", "C0"}, {"Q1", "C1"}, {"Q2", "C2"}, {"Q3", "C3"}});
            b.add("shift4", {{"D", "HIGH"}, {"CLK", "CLK"}, {"Q0", "S0"}, {"Q1", "S1"}, {"Q2", "S2"}, {"Q3", "S3"}});
            const auto r = simulate(b, ".tran 1u 18.2m 0 1u uic",
                {"time", "t", "tn", "d", "jk", "l", "l0", "c0", "c1", "c2", "c3", "s0", "s1", "s2", "s3"});
            REQUIRE(r.count("time") && r.count("c3"));
            CHECK(bit(at(r, "t", 0.25e-3)) == 0); // no edge yet: everything starts low
            CHECK(bit(at(r, "d", 0.25e-3)) == 0);
            CHECK(bit(at(r, "l", 0.75e-3)) == 1);  // a latch follows D while EN is high
            CHECK(bit(at(r, "l0", 18e-3)) == 0); // and keeps its value while EN is low
            CHECK(bit(at(r, "d", 1e-3)) == 1);
            int counter_errors = 0;
            int toggle_errors = 0;
            for (int n = 1; n <= 18; ++n) {
                const double t = n * 1e-3;
                toggle_errors += bit(at(r, "t", t)) != n % 2 || bit(at(r, "tn", t)) != 1 - n % 2;
                toggle_errors += bit(at(r, "jk", t)) != n % 2;
                const int count = bit(at(r, "c0", t)) + 2 * bit(at(r, "c1", t)) + 4 * bit(at(r, "c2", t))
                    + 8 * bit(at(r, "c3", t));
                counter_errors += count != n % 16;
            }
            CHECK(toggle_errors == 0);
            CHECK(counter_errors == 0);
            const int shifted[] = {bit(at(r, "s0", 2e-3)), bit(at(r, "s1", 2e-3)), bit(at(r, "s2", 2e-3)),
                bit(at(r, "s3", 2e-3))};
            CHECK((shifted[0] == 1 && shifted[1] == 1 && shifted[2] == 0 && shifted[3] == 0)); // two edges
            CHECK(bit(at(r, "s3", 4e-3)) == 1);
        }
        {
            auto sr = [&](const char* s, const char* r) {
                test::CircuitBuilder b(components());
                supply(b, "S", s);
                supply(b, "R", r);
                b.add("sr_latch", {{"S", "S"}, {"R", "R"}, {"Q", "Q"}, {"QN", "QN"}});
                return op(b, "q");
            };
            CHECK(close_to(sr("5", "0"), 5.0, 0.01));
            CHECK(close_to(sr("0", "5"), 0.0, 0.01));
            CHECK(close_to(sr("5", "5"), 0.0, 0.01)); // reset wins
        }
    }
    { // SN-202: BCD decoders for 7-segment displays, the display's segment currents, a decade counter
        // The chips draw 6 and 9 without tails; the 7447 has symbols for 10-14; the hex decoders show A-F.
        const std::map<int, std::string> chip{{0, "abcdef"}, {1, "bc"}, {2, "abdeg"}, {3, "abcdg"}, {4, "bcfg"},
            {5, "acdfg"}, {6, "cdefg"}, {7, "abc"}, {8, "abcdefg"}, {9, "abcfg"}};
        auto ttl = chip;
        ttl.insert({{10, "deg"}, {11, "cdg"}, {12, "bfg"}, {13, "adfg"}, {14, "defg"}});
        const std::map<int, std::string> hex{{0, "abcdef"}, {1, "bc"}, {2, "abdeg"}, {3, "abcdg"}, {4, "bcfg"},
            {5, "acdfg"}, {6, "acdefg"}, {7, "abc"}, {8, "abcdefg"}, {9, "abcdfg"}, {10, "abcefg"}, {11, "cdefg"},
            {12, "adef"}, {13, "bcdeg"}, {14, "adefg"}, {15, "aefg"}};
        // Segments lit for one input code; `controls` sets other inputs ("LT" -> "0").
        auto segments = [&](const char* type, int code, const std::map<std::string, std::string>& controls) {
            test::CircuitBuilder b(components());
            const bool cmos = std::string(type) == "decoder_4511";
            const bool ttl_part = std::string(type) == "decoder_7447";
            const bool low = ttl_part || std::string(type) == "decoder_hex_ca";
            supply(b, "VP", "5");
            std::map<std::string, std::string> pins{{"A", "A"}, {"B", "B"}, {"C", "C"}, {"D", "D"},
                {cmos ? "VDD" : "VCC", "VP"}, {cmos ? "VSS" : "GND", "0"}};
            const char* bits[] = {"A", "B", "C", "D"};
            for (int i = 0; i < 4; ++i) {
                supply(b, bits[i], ((code >> i) & 1) ? "5" : "0");
            }
            if (cmos) { // LT and BI high, LE low unless the test says otherwise
                for (const auto& [pin, level] : std::map<std::string, std::string>{{"LT", "5"}, {"BI", "5"}, {"LE", "0"}}) {
                    const auto c = controls.find(pin);
                    supply(b, pin.c_str(), c == controls.end() ? level.c_str() : c->second.c_str());
                    pins[pin] = pin;
                }
            } else {
                for (const auto& [pin, level] : controls) {
                    supply(b, pin.c_str(), level.c_str());
                    pins[pin] = pin;
                }
            }
            std::vector<std::string> vectors;
            for (const char s : std::string("abcdefg")) {
                pins[std::string(1, s)] = std::string("S") + s;
                // Active-low outputs get a pull-up (the 7447 only sinks), the others a load to ground.
                b.add("resistor", {{"1", std::string("S") + s}, {"2", low ? "VP" : "0"}}, {{"R", "1k"}});
                vectors.push_back(std::string("s") + s);
            }
            b.add(type, pins);
            const auto r = simulate(b, ".op", vectors);
            std::string on;
            for (const char s : std::string("abcdefg")) {
                const auto it = r.find(std::string("s") + s);
                const double v = it == r.end() || it->second.empty() ? -1 : it->second.front();
                if ((v > 2.5) != low && v >= 0) {
                    on += s;
                }
            }
            return on;
        };
        int wrong = 0;
        for (int code = 0; code < 16; ++code) {
            const auto expect = [code](const std::map<int, std::string>& table) {
                return table.count(code) ? table.at(code) : std::string();
            };
            for (const auto& [type, table] : std::vector<std::pair<const char*, const std::map<int, std::string>*>>{
                     {"decoder_4511", &chip}, {"decoder_7447", &ttl}, {"decoder_hex_cc", &hex}, {"decoder_hex_ca", &hex}}) {
                const auto got = segments(type, code, {});
                if (got != expect(*table)) {
                    std::printf("  %s, code %d: segments %s, expected %s\n", type, code, got.c_str(), expect(*table).c_str());
                    ++wrong;
                }
            }
        }
        CHECK(wrong == 0);
        // Lamp test lights everything, blanking turns everything off (4511 and 7447).
        CHECK(segments("decoder_4511", 1, {{"LT", "0"}}) == "abcdefg");
        CHECK(segments("decoder_4511", 8, {{"BI", "0"}}).empty());
        CHECK(segments("decoder_7447", 1, {{"LT", "0"}}) == "abcdefg");
        CHECK(segments("decoder_7447", 8, {{"BI/RBO", "0"}}).empty());
        // RBI low blanks a zero (leading-zero blanking) but not other digits.
        CHECK(segments("decoder_7447", 0, {{"RBI", "0"}}).empty());
        CHECK(segments("decoder_7447", 3, {{"RBI", "0"}}) == "abcdg");
        test::CircuitBuilder d(components());
        supply(d, "VS", "5");
        d.add("resistor", {{"1", "VS"}, {"2", "SA"}}, {{"R", "330"}});
        d.add("seven_segment_cc", {{"a", "SA"}, {"COM", "0"}}); // other segments open
        const auto r = simulate(d, ".op", {"v_ds1_sa#branch"});
        REQUIRE(r.count("v_ds1_sa#branch"));
        const double current = r.at("v_ds1_sa#branch").front();
        std::printf("  7-segment display, segment a from 5 V through 330 ohm: %.2f mA\n", current * 1e3);
        CHECK(current > 8e-3 && current < 10.5e-3);

        test::CircuitBuilder c(components());
        c.add("vpulse", {{"+", "CLK"}, {"-", "0"}}, {{"Vlow", "5"}, {"Vhigh", "0"}, {"f", "1k"}, {"rise", "1u"}});
        c.add("counter10", {{"CLK", "CLK"}, {"Q0", "C0"}, {"Q1", "C1"}, {"Q2", "C2"}, {"Q3", "C3"}});
        const auto t = simulate(c, ".tran 1u 12.2m 0 1u uic", {"time", "c0", "c1", "c2", "c3"});
        REQUIRE(t.count("time"));
        int count_errors = 0;
        for (int n = 1; n <= 12; ++n) {
            std::size_t i = 0;
            while (i < t.at("time").size() && t.at("time")[i] < n * 1e-3) {
                ++i;
            }
            auto bit = [&](const char* v) { return t.at(v)[i] > 2.5 ? 1 : 0; };
            count_errors += bit("c0") + 2 * bit("c1") + 4 * bit("c2") + 8 * bit("c3") != n % 10;
        }
        CHECK(count_errors == 0);
    }
    { // SN-201: regulators and references
        auto regulator = [&](const char* type, const char* vin, const char* load) {
            test::CircuitBuilder b(components());
            supply(b, "IN", vin);
            b.add(type, {{"IN", "IN"}, {"GND", "0"}, {"OUT", "OUT"}});
            b.add("resistor", {{"1", "OUT"}, {"2", "0"}}, {{"R", load}});
            return op(b, "out");
        };
        const double v5 = regulator("regulator_78xx", "12", "100");
        const double dropout = regulator("regulator_78xx", "6", "100");
        const double limited = regulator("regulator_78xx", "12", "1");
        const double ldo = regulator("regulator_ldo", "5", "33");
        std::printf("  7805: %.4f V at 50 mA, %.3f V from 6 V, %.3f V into 1 ohm; LDO %.4f V\n", v5, dropout, limited, ldo);
        CHECK(close_to(v5, 5.0, 0.01));
        CHECK(close_to(dropout, 4.0, 0.02));    // 6 V - 2 V dropout
        CHECK(close_to(limited, 1.0, 0.02));    // 1 A current limit
        CHECK(close_to(ldo, 3.3, 0.01));
        test::CircuitBuilder adj(components());
        supply(adj, "IN", "12");
        adj.add("lm317", {{"IN", "IN"}, {"ADJ", "ADJ"}, {"OUT", "OUT"}});
        adj.add("resistor", {{"1", "OUT"}, {"2", "ADJ"}}, {{"R", "240"}});
        adj.add("resistor", {{"1", "ADJ"}, {"2", "0"}}, {{"R", "720"}});
        CHECK(close_to(op(adj, "out"), 1.25 * (1 + 720.0 / 240.0) + 50e-6 * 720, 0.01)); // 5.036 V
        auto shunt = [&](bool divider) {
            test::CircuitBuilder b(components());
            supply(b, "VS", "12");
            b.add("resistor", {{"1", "VS"}, {"2", "K"}}, {{"R", "1k"}});
            if (divider) { // Vk = 2.495 V x (1 + 10k/10k)
                b.add("resistor", {{"1", "K"}, {"2", "REF"}}, {{"R", "10k"}});
                b.add("resistor", {{"1", "REF"}, {"2", "0"}}, {{"R", "10k"}});
                b.add("tl431", {{"K", "K"}, {"A", "0"}, {"REF", "REF"}});
            } else {
                b.add("tl431", {{"K", "K"}, {"A", "0"}, {"REF", "K"}});
            }
            return op(b, "k");
        };
        CHECK(close_to(shunt(false), 2.495, 0.005));
        CHECK(close_to(shunt(true), 4.99, 0.01));
    }
    { // transformer 10:1 with a 10 V, 50 Hz primary: about 1 V on the secondary
        test::CircuitBuilder b(components());
        b.add("vsine", {{"+", "P"}, {"-", "0"}}, {{"amplitude", "10"}, {"f", "50"}});
        b.add("transformer", {{"P1", "P"}, {"P2", "0"}, {"S1", "S"}, {"S2", "0"}}, {{"n", "10"}, {"Lp", "10"}});
        b.add("resistor", {{"1", "S"}, {"2", "0"}}, {{"R", "1k"}});
        const auto r = simulate(b, ".tran 50u 60m", {"time", "s"});
        REQUIRE(r.count("s") && r.count("time"));
        double peak = 0;
        for (std::size_t i = 0; i < r.at("s").size(); ++i) {
            if (r.at("time")[i] > 0.04) {
                peak = std::max(peak, r.at("s")[i]);
            }
        }
        CHECK(close_to(peak, 1.0, 0.05));
    }
}
