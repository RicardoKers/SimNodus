// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// Blocks drawn by the user (PRJ-06, ADR 0098): symbol from the ports, the
// circuit in place of the block in the netlist, file and clipboard round trips.
#include "test_support.hpp"

#include "circuit_builder.hpp"
#include "editing/operations.hpp"
#include "io/project_file.hpp"
#include "library/blocks.hpp"
#include "model/edit.hpp"
#include "netlist/netlist.hpp"

#include <algorithm>
#include <cstdio>
#include <memory>

using namespace simnodus;

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

// A divider: IN -1k- OUT -1k- ground, with ports IN (left) and OUT (right).
model::Schematic divider()
{
    test::CircuitBuilder b(parts());
    b.add("port", {{"P", "IN"}}, {{"name", "IN"}, {"side", "left"}});
    b.add("port", {{"P", "OUT"}}, {{"name", "OUT"}, {"side", "right"}});
    b.add("resistor", {{"1", "IN"}, {"2", "OUT"}}, {{"R", "1k"}});
    b.add("resistor", {{"1", "OUT"}, {"2", "0"}}, {{"R", "1k"}});
    b.add("ground", {{"0", "0"}});
    return b.schematic;
}

model::Block block(const std::string& name, model::Schematic circuit)
{
    return {name, name + ".snx", std::make_shared<const model::Schematic>(std::move(circuit))};
}

const netlist::Element* element(const netlist::Netlist& n, const std::string& name)
{
    const auto it = std::find_if(n.elements.begin(), n.elements.end(),
        [&](const netlist::Element& e) { return e.name == name; });
    return it == n.elements.end() ? nullptr : &*it;
}

bool has_code(const netlist::Netlist& n, const std::string& code)
{
    return std::any_of(n.diagnostics.begin(), n.diagnostics.end(),
        [&](const netlist::Diagnostic& d) { return d.code == code; });
}

} // namespace

TEST_CASE("blocks: the symbol has the ports as pins, in schematic order")
{
    test::CircuitBuilder b(parts());
    b.add("port", {}, {{"name", "B"}, {"side", "left"}});
    b.add("port", {}, {{"name", "VCC"}, {"side", "top"}});
    b.add("port", {}, {{"name", "Y"}, {"side", "right"}});
    b.add("port", {}, {{"name", "A"}, {"side", "left"}});
    b.add("resistor", {});
    // Ports placed from left to right at y = 0: move A above B so that it comes first.
    auto s = b.schematic;
    for (const auto& c : b.schematic.components()) {
        if (c.parameters.count("name") && c.parameters.at("name") == "A") {
            editing::move_items(s, parts(), {c.id}, {0, -10}).apply(s);
        }
    }
    std::vector<std::string> problems;
    const auto d = library::block_definition(block("Gate", s), parts(), problems);
    REQUIRE(d.has_value());
    CHECK(problems.empty());
    CHECK(d->type == "block:Gate");
    CHECK(d->category == "Blocks");
    REQUIRE(d->pins.size() == 4);
    const auto* a = d->pin("A");
    const auto* bp = d->pin("B");
    const auto* y = d->pin("Y");
    const auto* vcc = d->pin("VCC");
    REQUIRE((a && bp && y && vcc));
    CHECK(a->direction == library::PinDirection::Left);
    CHECK(a->at.y < bp->at.y);       // A above B, as on the block's schematic
    CHECK(bp->at.y - a->at.y == 2);  // two grid units apart
    CHECK(a->at.x == bp->at.x);
    CHECK(y->at.x > 0);
    CHECK(vcc->direction == library::PinDirection::Up);
    CHECK(vcc->at.y < a->at.y);
    CHECK(d->block != nullptr);
}

TEST_CASE("blocks: circuits that cannot be blocks say why")
{
    std::vector<std::string> problems;
    test::CircuitBuilder none(parts());
    none.add("resistor", {{"1", "A"}, {"2", "0"}});
    CHECK(!library::block_definition(block("Plain", none.schematic), parts(), problems));
    REQUIRE(problems.size() == 1);
    CHECK(problems[0].find("no ports") != std::string::npos);

    problems.clear();
    test::CircuitBuilder ground(parts());
    ground.add("port", {}, {{"name", "GND"}});
    CHECK(!library::block_definition(block("Grounded", ground.schematic), parts(), problems));
    CHECK(!problems.empty());

    problems.clear();
    test::CircuitBuilder board(parts());
    board.add("port", {}, {{"name", "IO"}});
    board.add("bluepill", {});
    CHECK(!library::block_definition(block("Board", board.schematic), parts(), problems));
    CHECK(!problems.empty());

    // The project's library leaves such a block out and explains it.
    model::Schematic project;
    project.set_blocks({{"Plain", block("Plain", none.schematic)}, {"Divider", block("Divider", divider())}});
    std::vector<std::string> why;
    const auto library = library::with_blocks(parts(), project, &why);
    CHECK(library.find("block:Divider") != nullptr);
    CHECK(library.find("block:Plain") == nullptr);
    CHECK(why.size() == 1);
}

TEST_CASE("blocks: the netlist puts the block's circuit in its place")
{
    model::Schematic project;
    project.set_blocks({{"Divider", block("Divider", divider())}});
    const auto library = library::with_blocks(parts(), project);
    test::CircuitBuilder b(library);
    b.schematic = project;
    b.add("vdc", {{"+", "VS"}, {"-", "0"}}, {{"V", "10"}});
    const auto u1 = b.add("block:Divider", {{"IN", "VS"}, {"OUT", "HALF"}});
    const auto u2 = b.add("block:Divider", {{"IN", "HALF"}, {"OUT", "QUARTER"}});
    REQUIRE((u1 != model::no_id && u2 != model::no_id));

    const auto n = netlist::generate_netlist(b.schematic, library, {false, 0.0, {}});
    for (const auto& d : n.diagnostics) {
        if (d.severity == netlist::Severity::Error) {
            std::printf("  unexpected: %s\n", d.message.c_str());
        }
        CHECK(d.severity != netlist::Severity::Error);
    }
    // Two instances, each with its own resistors, joined to the nets around them.
    const auto* r1 = element(n, "R_U1_R1");
    const auto* r2 = element(n, "R_U1_R2");
    const auto* r3 = element(n, "R_U2_R1");
    REQUIRE((r1 && r2 && r3));
    CHECK((r1->nodes == std::vector<std::string>{"VS", "HALF"}));
    CHECK((r2->nodes == std::vector<std::string>{"HALF", "0"}));
    CHECK((r3->nodes == std::vector<std::string>{"HALF", "QUARTER"}));
    // The nets of the schematic keep the names the editor gives them.
    const auto plain = netlist::build_connectivity(b.schematic, library);
    for (const auto& [pin, net] : plain.pin_net) {
        const auto it = n.connectivity.pin_net.find(pin);
        REQUIRE(it != n.connectivity.pin_net.end());
        CHECK(n.connectivity.nets[static_cast<std::size_t>(it->second)].name
            == plain.nets[static_cast<std::size_t>(net)].name);
    }
    CHECK(n.pin_node.at({u1, "OUT"}) == "HALF");
}

TEST_CASE("blocks: unnamed nets inside keep the names outside unchanged")
{
    // Inside: IN -R- (unnamed) -R- OUT; outside: unnamed nets on both pins.
    test::CircuitBuilder inner(parts());
    const auto p_in = inner.add("port", {}, {{"name", "IN"}});
    const auto r1 = inner.add("resistor", {}, {{"R", "1k"}});
    const auto r2 = inner.add("resistor", {}, {{"R", "1k"}});
    const auto p_out = inner.add("port", {}, {{"name", "OUT"}, {"side", "right"}});
    // Wires leave a pin away from its part (up from a top pin, down from a
    // bottom one), each at its own distance so that no two of them overlap.
    int distance = 0;
    auto connect = [&distance](model::Schematic& s, const library::ComponentLibrary& library, model::Id a,
                       const char* pa, model::Id z, const char* pz) {
        auto pin_at = [&](model::Id id, const char* pin) {
            const auto* c = s.component(id);
            const auto* d = library.find(c->type);
            return library::pin_position(*d, *d->pin(pin), *c);
        };
        distance += 10;
        const auto from = pin_at(a, pa);
        const auto to = pin_at(z, pz);
        const int level_from = from.y > 0 ? from.y + distance : from.y - distance;
        const int level_to = to.y > 0 ? to.y + distance : to.y - distance;
        const int middle = (from.x + to.x) / 2;
        editing::add_wire(s,
            {from, {from.x, level_from}, {middle, level_from}, {middle, level_to}, {to.x, level_to}, to})
            .apply(s);
    };
    connect(inner.schematic, parts(), p_in, "P", r1, "1");
    connect(inner.schematic, parts(), r1, "2", r2, "1");
    connect(inner.schematic, parts(), r2, "2", p_out, "P");
    model::Schematic project;
    project.set_blocks({{"Chain", block("Chain", inner.schematic)}});
    const auto library = library::with_blocks(parts(), project);
    test::CircuitBuilder b(library);
    b.schematic = project;
    b.add("vdc", {{"+", "VS"}, {"-", "0"}});
    const auto u1 = b.add("block:Chain", {{"IN", "VS"}});
    const auto load = b.add("resistor", {{"2", "0"}});
    // Wire the block's OUT to the load: an unnamed net, N001 in the editor.
    connect(b.schematic, library, u1, "OUT", load, "1");

    const auto plain = netlist::build_connectivity(b.schematic, library);
    const auto n = netlist::generate_netlist(b.schematic, library, {false, 0.0, {}});
    CHECK(n.ok());
    const auto out_net = plain.nets[static_cast<std::size_t>(plain.pin_net.at({u1, "OUT"}))].name;
    CHECK(out_net == "N001");
    CHECK(n.pin_node.at({u1, "OUT"}) == "N001");
    CHECK(n.pin_node.at({load, "1"}) == "N001");
    const auto* inner_r = element(n, "R_U1_R1");
    REQUIRE(inner_r);
    CHECK(inner_r->nodes[0] == "VS");
    CHECK(inner_r->nodes[1] == "U1_N001"); // the inside net, named after the block
}

TEST_CASE("blocks: findings inside a block point at the block")
{
    test::CircuitBuilder inner(parts());
    inner.add("port", {{"P", "IN"}}, {{"name", "IN"}});
    inner.add("resistor", {{"1", "IN"}}, {{"R", "abc"}}); // not a number; pin 2 open
    model::Schematic project;
    project.set_blocks({{"Broken", block("Broken", inner.schematic)}});
    const auto library = library::with_blocks(parts(), project);
    test::CircuitBuilder b(library);
    b.schematic = project;
    b.add("vdc", {{"+", "VS"}, {"-", "0"}});
    const auto u1 = b.add("block:Broken", {{"IN", "VS"}});
    const auto n = netlist::generate_netlist(b.schematic, library, {false, 0.0, {}});
    CHECK(!n.ok());
    bool found = false;
    for (const auto& d : n.diagnostics) {
        if (d.code == "invalid-value") {
            found = true;
            CHECK((d.items == std::vector<model::Id>{u1}));
            CHECK(d.message.rfind("In block U1: U1.R1:", 0) == 0);
        }
    }
    CHECK(found);
}

TEST_CASE("blocks: blocks inside blocks and switches inside a block")
{
    // Quarter = two dividers in a row; a switch inside stays as saved.
    model::Schematic quarter_project;
    quarter_project.set_blocks({{"Divider", block("Divider", divider())}});
    const auto inner_library = library::with_blocks(parts(), quarter_project);
    test::CircuitBuilder q(inner_library);
    q.schematic = quarter_project;
    q.add("port", {{"P", "IN"}}, {{"name", "IN"}});
    q.add("port", {{"P", "OUT"}}, {{"name", "OUT"}, {"side", "right"}});
    q.add("spst", {{"1", "IN"}, {"2", "A"}}, {{"closed", "closed"}});
    q.add("block:Divider", {{"IN", "A"}, {"OUT", "M"}});
    q.add("block:Divider", {{"IN", "M"}, {"OUT", "OUT"}});

    model::Schematic project;
    project.set_blocks({{"Quarter", block("Quarter", q.schematic)}});
    const auto library = library::with_blocks(parts(), project);
    test::CircuitBuilder b(library);
    b.schematic = project;
    b.add("vdc", {{"+", "VS"}, {"-", "0"}});
    const auto u1 = b.add("block:Quarter", {{"IN", "VS"}, {"OUT", "Q"}});
    const auto n = netlist::generate_netlist(b.schematic, library, {true, 0.0, {}});
    for (const auto& d : n.diagnostics) {
        if (d.severity == netlist::Severity::Error) {
            std::printf("  unexpected: %s\n", d.message.c_str());
        }
        CHECK(d.severity != netlist::Severity::Error);
    }
    CHECK(element(n, "R_U1_U2_R1") != nullptr); // U1.U2.R1: the second divider inside
    const auto* last = element(n, "R_U1_U2_R2");
    REQUIRE(last);
    CHECK(last->nodes[0] == "Q");
    CHECK(has_code(n, "block-control"));
    REQUIRE(n.controls.size() == 1);
    CHECK(n.controls[0].initial == 1.0);
    CHECK(n.pin_node.at({u1, "IN"}) == "VS");
}

TEST_CASE("blocks: a port connects by name like a label")
{
    test::CircuitBuilder b(parts());
    const auto p = b.add("port", {}, {{"name", "IN"}});
    b.add("vdc", {{"+", "IN"}, {"-", "0"}});
    const auto c = netlist::build_connectivity(b.schematic, parts());
    CHECK(c.nets[static_cast<std::size_t>(c.pin_net.at({p, "P"}))].name == "IN");
    CHECK(std::find(c.unconnected_pins.begin(), c.unconnected_pins.end(), netlist::PinRef{p, "P"})
        == c.unconnected_pins.end());
}

TEST_CASE("blocks: projects keep their blocks; format 1 without them")
{
    model::Schematic project;
    CHECK(io::to_json_text(project).find("\"version\": 1") != std::string::npos);
    model::Schematic quarter;
    quarter.set_blocks({{"Divider", block("Divider", divider())}});
    project.set_blocks({{"Divider", block("Divider", divider())}, {"Quarter", block("Quarter", quarter)}});
    const std::string text = io::to_json_text(project);
    CHECK(text.find("\"version\": 2") != std::string::npos);
    const auto loaded = io::from_json_text(text);
    REQUIRE(loaded.schematic);
    CHECK(*loaded.schematic == project);
    const auto* q = loaded.schematic->block("Quarter");
    REQUIRE(q);
    CHECK(q->source == "Quarter.snx");
    CHECK(q->schematic->block("Divider") != nullptr);
    CHECK(io::to_json_text(*loaded.schematic) == text);
}

TEST_CASE("blocks: changing blocks is undoable; copy and paste take them along")
{
    model::Schematic s;
    model::Schematic changed = s;
    changed.set_blocks({{"Divider", block("Divider", divider())}});
    const auto edit = model::make_edit(s, changed, "Add block");
    CHECK(!edit.empty());
    edit.apply(s);
    CHECK(s.block("Divider") != nullptr);
    edit.revert(s);
    CHECK(s.blocks().empty());

    edit.apply(s);
    const auto library = library::with_blocks(parts(), s);
    model::Id u1 = model::no_id;
    editing::add_component(s, *library.find("block:Divider"), {0, 0}, {}, &u1).apply(s);
    const std::string clipboard = editing::copy_items(s, {u1});
    model::Schematic other;
    std::set<model::Id> created;
    editing::paste_items(other, library, clipboard, {4, 4}, &created).apply(other);
    CHECK(created.size() == 1);
    CHECK(other.block("Divider") != nullptr);
}
