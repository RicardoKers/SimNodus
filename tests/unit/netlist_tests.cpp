// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "test_support.hpp"

#include "circuit_builder.hpp"
#include "netlist/netlist.hpp"

#include <algorithm>

using namespace simnodus;
using namespace simnodus::model;
using netlist::Severity;

namespace {

library::ComponentLibrary load()
{
    library::ComponentLibrary library;
    library.load_directory(SIMNODUS_TEST_LIBRARY_DIR);
    return library;
}

bool has(const netlist::Netlist& n, const std::string& code)
{
    return std::any_of(n.diagnostics.begin(), n.diagnostics.end(),
        [&](const netlist::Diagnostic& d) { return d.code == code; });
}

void dump(const netlist::Netlist& n)
{
    for (const auto& d : n.diagnostics) {
        std::printf("  [%s] %s\n", d.code.c_str(), d.message.c_str());
    }
    for (const auto& l : n.lines) {
        std::printf("  | %s\n", l.c_str());
    }
}

} // namespace

TEST_CASE("wires connect at ends and T junctions but not at crossings")
{
    const auto lib = load();
    Schematic s;
    Id a = 0, b = 0, c = 0;
    editing::add_wire(s, {{0, 0}, {10, 0}}, &a).apply(s);
    editing::add_wire(s, {{5, 0}, {5, 8}}, &b).apply(s);   // T onto a
    editing::add_wire(s, {{8, -4}, {8, 4}}, &c).apply(s);  // crosses a
    const auto conn = netlist::build_connectivity(s, lib);
    CHECK(conn.wire_net.at(a) == conn.wire_net.at(b));
    CHECK(conn.wire_net.at(a) != conn.wire_net.at(c));
    CHECK((conn.junctions == std::vector<Point>{{5, 0}}));
    // Open ends: (0,0), (10,0), (5,8) and both ends of c.
    CHECK(conn.dangling_ends.size() == 5);
}

TEST_CASE("labels join nets by name and ground is node 0")
{
    const auto lib = load();
    test::CircuitBuilder b(lib);
    const Id r1 = b.add("resistor", {{"1", "IN"}, {"2", "MID"}});
    const Id r2 = b.add("resistor", {{"1", "MID"}, {"2", "GND"}});
    const Id gnd = b.add("ground", {});
    (void)gnd;
    const auto conn = netlist::build_connectivity(b.schematic, lib);
    CHECK(conn.net_of_pin({r1, "2"}) == conn.net_of_pin({r2, "1"}));
    CHECK(conn.net_of_pin({r2, "2"})->name == "0");
    CHECK(conn.net_of_pin({r2, "2"})->ground);
    CHECK(conn.net_of_pin({r1, "1"})->name == "IN");
    CHECK(conn.unconnected_pins.empty()); // labels count as connections
}

TEST_CASE("net names are unique ignoring case")
{
    const auto lib = load();
    test::CircuitBuilder b(lib);
    const Id r1 = b.add("resistor", {{"1", "out"}, {"2", "0"}});
    const Id r2 = b.add("resistor", {{"1", "OUT"}, {"2", "0"}});
    const auto conn = netlist::build_connectivity(b.schematic, lib);
    const auto* n1 = conn.net_of_pin({r1, "1"});
    const auto* n2 = conn.net_of_pin({r2, "1"});
    CHECK(n1 != n2);
    CHECK(n1->name != n2->name);
}

TEST_CASE("model parameters choose a built-in model or one kept in the component")
{
    const auto lib = load();
    test::CircuitBuilder b(lib);
    b.add("vdc", {{"+", "VCC"}, {"-", "0"}}, {{"V", "5"}});
    const Id q1 = b.add("npn", {{"C", "VCC"}, {"B", "B"}, {"E", "0"}}, {{"model", "2N2222A"}});
    const Id q2 = b.add("npn", {{"C", "VCC"}, {"B", "B"}, {"E", "0"}});
    b.add("resistor", {{"1", "VCC"}, {"2", "B"}}, {{"R", "100k"}});
    auto n = netlist::generate_netlist(b.schematic, lib);
    if (!n.ok()) {
        dump(n);
    }
    REQUIRE(n.ok());
    CHECK(std::find(n.lines.begin(), n.lines.end(), "Q_Q1 VCC B 0 BM_NPN_2N2222A") != n.lines.end());
    CHECK(std::find(n.lines.begin(), n.lines.end(), "Q_Q2 VCC B 0 BM_NPN_Generic") != n.lines.end());
    CHECK(n.models.size() == 2);
    CHECK(n.models.front().rfind(".model BM_NPN_2N2222A NPN(", 0) == 0);
    // A model from the user's folder travels inside the component.
    editing::set_model(b.schematic, q2, "model", "MY-NPN", ".model MY-NPN NPN(BF=50)").apply(b.schematic);
    n = netlist::generate_netlist(b.schematic, lib);
    REQUIRE(n.ok());
    CHECK(std::find(n.lines.begin(), n.lines.end(), "Q_Q2 VCC B 0 UM_Q2_model") != n.lines.end());
    CHECK(std::find(n.models.begin(), n.models.end(), ".model UM_Q2_model NPN(BF=50)") != n.models.end());
    // Parameters ngspice cannot use are left out of the deck, with a warning for the part.
    editing::set_model(b.schematic, q2, "model", "MY-NPN", ".model MY-NPN NPN(BF=50 IS=abc FOO=1)").apply(b.schematic);
    n = netlist::generate_netlist(b.schematic, lib);
    CHECK(n.ok());
    CHECK(has(n, "model-parameter"));
    CHECK(std::find(n.models.begin(), n.models.end(), ".model UM_Q2_model NPN(BF=50)") != n.models.end());
    // An unknown model, or one of the wrong type, is an error that names the part.
    editing::set_parameter(b.schematic, q1, "model", "NOPE").apply(b.schematic);
    n = netlist::generate_netlist(b.schematic, lib);
    CHECK(has(n, "unknown-model"));
    editing::set_model(b.schematic, q1, "model", "D1", ".model D1 D(IS=1n)").apply(b.schematic);
    CHECK(has(netlist::generate_netlist(b.schematic, lib), "unknown-model"));
}

TEST_CASE("splitter bit pins join nets by bus name; buses connect nothing by themselves")
{
    const auto lib = load();
    Schematic s;
    // R1 from VCC to the first splitter's bit 0; R2 from the second splitter's
    // bit 0 to ground; R3 on bit 1 of the first splitter only.
    Id r1 = 0, r2 = 0, r3 = 0;
    editing::add_component(s, *lib.find("resistor"), {0, 0}, {}, &r1).apply(s);  // pins (0,-3), (0,3)
    editing::add_component(s, *lib.find("resistor"), {20, 0}, {}, &r2).apply(s); // pins (20,-3), (20,3)
    editing::add_component(s, *lib.find("resistor"), {4, 8}, {}, &r3).apply(s);  // pins (4,5), (4,11)
    editing::add_label(s, {0, -3}, "VCC").apply(s);
    editing::add_label(s, {20, 3}, "0").apply(s);
    editing::add_label(s, {4, 11}, "0").apply(s);
    Id a = 0, b = 0;
    editing::add_splitter(s, {-2, 3}, "D", 0, 4, &a).apply(s);    // bits at (0,3), (0,5), (0,7), (0,9)
    editing::add_splitter(s, {18, -3}, "D", 0, 2, &b).apply(s);   // bits at (20,-3), (20,-1)
    editing::add_wire(s, {{0, 5}, {4, 5}}).apply(s);              // bit 1 of the first to R3
    editing::add_bus(s, {{-2, 3}, {-2, -10}, {18, -10}, {18, -3}}, "D").apply(s);
    CHECK((model::splitter_pins(*s.splitter(a)) == std::vector<Point>{{0, 3}, {0, 5}, {0, 7}, {0, 9}}));
    const auto conn = netlist::build_connectivity(s, lib);
    const auto& d0 = conn.nets[static_cast<std::size_t>(conn.pin_net.at({r1, "2"}))];
    CHECK(d0.name == "D0");
    CHECK(conn.pin_net.at({r1, "2"}) == conn.pin_net.at({r2, "1"})); // D0 on both splitters
    CHECK(conn.nets[static_cast<std::size_t>(conn.pin_net.at({r3, "1"}))].name == "D1");
    CHECK(conn.pin_net.at({r3, "1"}) != conn.pin_net.at({r1, "2"}));
    // A wire ending on a bit pin is not an open end; a pin on a bit pin is connected.
    CHECK(std::find(conn.dangling_ends.begin(), conn.dangling_ends.end(), Point{0, 5}) == conn.dangling_ends.end());
    CHECK(std::find(conn.unconnected_pins.begin(), conn.unconnected_pins.end(), netlist::PinRef{r2, "1"})
        == conn.unconnected_pins.end());
    CHECK(conn.net_at(s, lib, {0, 7}).has_value()); // hover name of an unused bit
    // Renaming the bus renames the splitters on it: the bits stay joined, as "E0".
    for (const auto& bus : s.buses()) {
        editing::rename_bus(s, bus.id, "E").apply(s);
    }
    CHECK(s.splitter(a)->bus == "E");
    CHECK(s.splitter(b)->bus == "E");
    const auto renamed = netlist::build_connectivity(s, lib);
    CHECK(renamed.nets[static_cast<std::size_t>(renamed.pin_net.at({r1, "2"}))].name == "E0");
    const auto n = netlist::generate_netlist(s, lib);
    if (!n.ok()) {
        dump(n);
    }
    CHECK(n.ok());
}

TEST_CASE("a vendor subcircuit replaces the part's template, wrapped and with its pins in port order")
{
    const auto lib = load();
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
    test::CircuitBuilder b(lib);
    b.add("vdc", {{"+", "VCC"}, {"-", "0"}}, {{"V", "5"}});
    b.add("vdc", {{"+", "IN"}, {"-", "0"}}, {{"V", "1"}});
    const Id u = b.add("opamp", {{"IN+", "IN"}, {"IN-", "FB"}, {"OUT", "OUT"}, {"V+", "VCC"}, {"V-", "0"}});
    b.add("resistor", {{"1", "OUT"}, {"2", "FB"}}, {{"R", "10k"}});
    b.add("resistor", {{"1", "FB"}, {"2", "0"}}, {{"R", "10k"}});
    auto n = netlist::generate_netlist(b.schematic, lib);
    REQUIRE(n.ok()); // the behavioral default: no subcircuit
    CHECK(std::none_of(n.lines.begin(), n.lines.end(), [](const std::string& l) { return l.rfind("X_", 0) == 0; }));
    editing::set_model(b.schematic, u, "model", "SN/OPAMP", opamp).apply(b.schematic);
    n = netlist::generate_netlist(b.schematic, lib);
    if (!n.ok()) {
        dump(n);
    }
    REQUIRE(n.ok());
    // Port order of op-amp subcircuits: in+ in- v+ v- out.
    CHECK(std::find(n.lines.begin(), n.lines.end(), "X_U1 IN FB VCC 0 OUT UX_U1_model") != n.lines.end());
    CHECK(std::none_of(n.lines.begin(), n.lines.end(), [](const std::string& l) { return l.rfind("E_U1", 0) == 0; }));
    REQUIRE(n.models.size() >= 4);
    CHECK(n.models.front() == ".subckt UX_U1_model p1 p2 p3 p4 p5");
    CHECK(std::find(n.models.begin(), n.models.end(), ".SUBCKT SNX 1 2 3 4 5") != n.models.end()); // the vendor's name is local
    CHECK(std::find(n.models.begin(), n.models.end(), "X1 p1 p2 p3 p4 p5 SNX") != n.models.end());
    CHECK(n.models.back() == ".ends UX_U1_model");
    // Another assignment of the ports (for a file with another order).
    editing::set_model_pins(b.schematic, u, "model", {"IN-", "IN+", "V+", "V-", "OUT"}).apply(b.schematic);
    n = netlist::generate_netlist(b.schematic, lib);
    CHECK(std::find(n.lines.begin(), n.lines.end(), "X_U1 FB IN VCC 0 OUT UX_U1_model") != n.lines.end());
    // A subcircuit with the wrong number of ports is an error for the part.
    editing::set_model(b.schematic, u, "model", "SNDARL", ".SUBCKT SNDARL 1 2 3\n"
    "* TERMINALS: C B E\n"
    "Q1 1 2 4 QD\n"
    "Q2 1 4 3 QD\n"
    "R1 2 4 10K\n"
    "R2 4 3 150\n"
    ".ENDS\n"
    ".MODEL QD NPN(BF=100 IS=1E-14)\n").apply(b.schematic);
    CHECK(has(netlist::generate_netlist(b.schematic, lib), "unknown-model"));
}

TEST_CASE("an RC circuit gives the expected netlist")
{
    const auto lib = load();
    test::CircuitBuilder b(lib);
    b.add("vdc", {{"+", "IN"}, {"-", "0"}}, {{"V", "3.3"}});
    b.add("resistor", {{"1", "IN"}, {"2", "OUT"}}, {{"R", "4k7"}});
    b.add("capacitor", {{"1", "OUT"}, {"2", "0"}}, {{"C", "100n"}});
    const auto n = netlist::generate_netlist(b.schematic, lib);
    if (!n.ok()) {
        dump(n);
    }
    CHECK(n.ok());
    CHECK((n.lines == std::vector<std::string>{"V_V1 IN 0 DC 3.3", "R_R1 IN OUT 4700", "C_C1 OUT 0 1e-07"}));
    CHECK(n.models.empty());
    REQUIRE(n.elements.size() == 3);
    CHECK(n.elements[2].letter == 'C');
    CHECK((n.elements[2].nodes == std::vector<std::string>{"OUT", "0"}));
    const auto deck = netlist::build_deck(n, "test", {".op"});
    CHECK(deck.front() == "* test");
    CHECK(deck.back() == ".end");
}

TEST_CASE("interactive parts get external control sources")
{
    const auto lib = load();
    test::CircuitBuilder b(lib);
    b.add("vdc", {{"+", "VCC"}, {"-", "0"}});
    const Id sw = b.add("button_no", {{"1", "VCC"}, {"2", "A"}});
    b.add("potentiometer", {{"1", "VCC"}, {"W", "W"}, {"2", "0"}}, {{"position", "25"}});
    b.add("spst", {{"1", "A"}, {"2", "0"}}, {{"closed", "closed"}});
    auto n = netlist::generate_netlist(b.schematic, lib);
    if (!n.ok()) {
        dump(n);
    }
    CHECK(n.ok());
    REQUIRE(n.controls.size() == 3);
    CHECK(n.controls[0].component == sw);
    CHECK(n.controls[0].source == "V_ctl_SW1");
    CHECK(n.controls[0].initial == 0.0);
    CHECK(n.controls[1].initial == 0.25);
    CHECK(n.controls[2].initial == 1.0);
    CHECK(std::find(n.lines.begin(), n.lines.end(), "V_ctl_SW1 ctl_SW1 0 DC 0 external") != n.lines.end());
    // ngspice adds DC to the external value: the session supplies the position.
    CHECK(std::find(n.lines.begin(), n.lines.end(), "V_ctl_RV1 ctl_RV1 0 DC 0 external") != n.lines.end());
    CHECK(std::find(n.lines.begin(), n.lines.end(), "V_ctl_SW2 ctl_SW2 0 DC 0 external") != n.lines.end());
    CHECK(std::find(n.lines.begin(), n.lines.end(), "S_SW1 VCC A ctl_SW1 0 SW_SN") != n.lines.end());
    CHECK(n.models.size() == 2); // switch models emitted once
    n = netlist::generate_netlist(b.schematic, lib, {false, 0.0, {}});
    CHECK(std::find(n.lines.begin(), n.lines.end(), "V_ctl_SW1 ctl_SW1 0 DC 0") != n.lines.end());
    CHECK(std::find(n.lines.begin(), n.lines.end(), "V_ctl_RV1 ctl_RV1 0 DC 0.25") != n.lines.end());
}

TEST_CASE("periodic sources keep their phase across segments")
{
    const auto lib = load();
    test::CircuitBuilder b(lib);
    b.add("vpulse", {{"+", "P"}, {"-", "0"}}, {{"f", "1k"}, {"duty", "50"}, {"rise", "1u"}});
    b.add("vsine", {{"+", "S"}, {"-", "0"}}, {{"f", "1k"}, {"phase", "0"}});
    b.add("resistor", {{"1", "P"}, {"2", "S"}});
    auto n = netlist::generate_netlist(b.schematic, lib, {true, 0.0, {}});
    CHECK(n.lines[0] == "V_V1 P 0 PULSE(0 5 0 1e-06 1e-06 0.000499 0.001)");
    CHECK(n.lines[1] == "V_V2 S 0 SIN(0 1 1000 0 0 0)");
    n = netlist::generate_netlist(b.schematic, lib, {true, 1.0003, {}});
    // 0.3 ms into the period: delay -0.3 ms, sine phase 108 degrees.
    CHECK(n.lines[0].rfind("V_V1 P 0 PULSE(0 5 -0.000", 0) == 0);
    CHECK(n.elements[0].value_text.find("-0.0003") != std::string::npos
        || n.elements[0].value_text.find("-0.000299999") != std::string::npos);
    CHECK(n.lines[1].rfind("V_V2 S 0 SIN(0 1 1000 0 0 10", 0) == 0);
}

TEST_CASE("pre-run checks find common mistakes")
{
    const auto lib = load();
    {
        test::CircuitBuilder b(lib);
        b.add("resistor", {{"1", "A"}, {"2", "B"}});
        const auto n = netlist::generate_netlist(b.schematic, lib);
        CHECK(has(n, "no-ground"));
        CHECK(!n.ok());
    }
    {
        test::CircuitBuilder b(lib);
        b.add("vdc", {{"+", "A"}, {"-", "0"}});
        b.add("capacitor", {{"1", "A"}, {"2", "B"}});
        b.add("capacitor", {{"1", "B"}, {"2", "0"}});
        const auto n = netlist::generate_netlist(b.schematic, lib);
        CHECK(has(n, "floating-net"));
    }
    {
        test::CircuitBuilder b(lib);
        b.add("vdc", {{"+", "A"}, {"-", "0"}});
        b.add("vdc", {{"+", "A"}, {"-", "0"}});
        const auto n = netlist::generate_netlist(b.schematic, lib);
        CHECK(has(n, "source-loop"));
    }
    {
        test::CircuitBuilder b(lib);
        b.add("vdc", {{"+", "0"}, {"-", "0"}});
        const auto n = netlist::generate_netlist(b.schematic, lib);
        CHECK(has(n, "shorted-source"));
    }
    {
        test::CircuitBuilder b(lib);
        b.add("vdc", {{"+", "A"}, {"-", "0"}});
        b.add("resistor", {{"1", "A"}, {"2", "0"}}, {{"R", "abc"}});
        b.add("resistor", {{"1", "A"}, {"2", "0"}}, {{"R", "-5"}});
        b.add("led", {{"A", "A"}, {"K", "0"}}, {{"color", "purple"}});
        const auto n = netlist::generate_netlist(b.schematic, lib);
        CHECK(std::count_if(n.diagnostics.begin(), n.diagnostics.end(),
                  [](const auto& d) { return d.code == "invalid-value" || d.code == "value-out-of-range"; })
            == 3);
    }
    {
        test::CircuitBuilder b(lib);
        const Id r1 = b.add("resistor", {{"1", "A"}, {"2", "0"}});
        b.add("resistor", {{"1", "A"}, {"2", "0"}});
        editing::set_designator(b.schematic, r1, "R2").apply(b.schematic);
        const auto n = netlist::generate_netlist(b.schematic, lib);
        CHECK(has(n, "duplicate-designator"));
    }
    {
        test::CircuitBuilder b(lib);
        b.add("vdc", {{"+", "A"}, {"-", "0"}});
        b.add("resistor", {{"1", "A"}}); // pin 2 left open
        const auto n = netlist::generate_netlist(b.schematic, lib);
        CHECK(has(n, "unconnected-pin"));
        CHECK(n.ok()); // a warning, not an error
    }
    {
        Schematic s;
        Component c;
        c.id = s.allocate_id();
        c.type = "flux_capacitor";
        c.designator = "X1";
        s.put(c);
        const auto n = netlist::generate_netlist(s, lib);
        CHECK(has(n, "unknown-type"));
    }
}

TEST_CASE("current probes insert a 0 V source in series with the pin")
{
    const auto lib = load();
    test::CircuitBuilder b(lib);
    b.add("vdc", {{"+", "IN"}, {"-", "0"}});
    const Id r = b.add("resistor", {{"1", "IN"}, {"2", "0"}});
    netlist::NetlistOptions options;
    options.current_probes = {{r, "1"}};
    const auto n = netlist::generate_netlist(b.schematic, lib, options);
    CHECK(n.ok());
    CHECK(std::find(n.lines.begin(), n.lines.end(), "R_R1 pm_R1_1 0 1000") != n.lines.end());
    CHECK(std::find(n.lines.begin(), n.lines.end(), "V_pm_R1_1 IN pm_R1_1 DC 0") != n.lines.end());
    CHECK(n.pin_current.at({r, "1"}) == "v_pm_r1_1#branch");
}

TEST_CASE("every library component generates a netlist line")
{
    const auto lib = load();
    for (const auto& d : lib.definitions()) {
        if (d.spice.empty()) {
            continue;
        }
        test::CircuitBuilder b(lib);
        std::map<std::string, std::string> nets;
        int i = 0;
        for (const auto& p : d.pins) {
            nets[p.name] = i++ == 0 ? "0" : "N" + std::to_string(i);
        }
        b.add(d.type, nets);
        const auto n = netlist::generate_netlist(b.schematic, lib);
        const bool values_ok = std::none_of(n.diagnostics.begin(), n.diagnostics.end(),
            [](const auto& diag) { return diag.code == "invalid-value" || diag.code == "value-out-of-range"; });
        if (!values_ok || n.lines.empty()) {
            std::printf("  %s\n", d.type.c_str());
            dump(n);
        }
        CHECK(values_ok);
        CHECK(!n.lines.empty());
    }
}

TEST_CASE("a Blue Pill gives its regulator and high-impedance pins, without unconnected-pin warnings")
{
    const auto lib = load();
    test::CircuitBuilder b(lib);
    b.add("bluepill", {{"G1", "0"}}, {{"firmware", "firmware/blink.elf"}});
    const auto n = netlist::generate_netlist(b.schematic, lib);
    if (!n.ok() || !n.diagnostics.empty()) {
        dump(n);
    }
    CHECK(n.ok());
    CHECK(!has(n, "unconnected-pin"));
    CHECK(std::find(n.lines.begin(), n.lines.end(), "V_U1_reg reg_U1 0 DC 3.3") != n.lines.end());
    const auto high_impedance = std::count_if(n.lines.begin(), n.lines.end(),
        [](const std::string& l) { return l.size() > 7 && l.compare(l.size() - 7, 7, " 100Meg") == 0; });
    if (high_impedance != 32) {
        dump(n);
    }
    CHECK(high_impedance == 32);
}
