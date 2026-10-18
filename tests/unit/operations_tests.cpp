// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "test_support.hpp"

#include "editing/operations.hpp"

using namespace simnodus;
using namespace simnodus::model;
using editing::is_orthogonal;

namespace {

library::ComponentLibrary test_library()
{
    library::ComponentLibrary library;
    library.load_directory(SIMNODUS_TEST_LIBRARY_DIR);
    return library;
}

// Applies an edit and returns it (so the test can revert it).
Edit applied(Schematic& s, Edit edit)
{
    edit.apply(s);
    return edit;
}

Point pin(const library::ComponentLibrary& lib, const Schematic& s, Id component, const std::string& name)
{
    const auto* c = s.component(component);
    const auto* d = lib.find(c->type);
    return library::pin_position(*d, *d->pin(name), *c);
}

} // namespace

TEST_CASE("placing components numbers designators and uses defaults")
{
    const auto lib = test_library();
    Schematic s;
    Id r1 = 0, r2 = 0;
    applied(s, editing::add_component(s, *lib.find("resistor"), {10, 10}, {}, &r1));
    applied(s, editing::add_component(s, *lib.find("resistor"), {20, 10}, {90, false}, &r2));
    CHECK(s.component(r1)->designator == "R1");
    CHECK(s.component(r2)->designator == "R2");
    CHECK(s.component(r2)->orientation.rotation == 90);
    CHECK(s.component(r1)->parameters.at("R") == "1k");
}

TEST_CASE("wires are normalized and must be orthogonal")
{
    Schematic s;
    Id w = 0;
    auto edit = editing::add_wire(s, {{0, 0}, {5, 0}, {5, 0}, {10, 0}, {10, 4}}, &w);
    REQUIRE(!edit.empty());
    edit.apply(s);
    CHECK((s.wire(w)->points == std::vector<Point>{{0, 0}, {10, 0}, {10, 4}}));
    CHECK(editing::add_wire(s, {{0, 0}, {3, 4}}).empty());
    CHECK(editing::add_wire(s, {{1, 1}, {1, 1}}).empty());
}

TEST_CASE("moving a component keeps attached wires connected and orthogonal")
{
    const auto lib = test_library();
    Schematic s;
    Id r = 0, straight = 0, bent = 0;
    applied(s, editing::add_component(s, *lib.find("resistor"), {0, 0}, {}, &r));
    // Pin 1 at (0,-3), pin 2 at (0,3).
    applied(s, editing::add_wire(s, {{0, -3}, {0, -10}}, &straight));
    applied(s, editing::add_wire(s, {{0, 3}, {0, 8}, {12, 8}}, &bent));
    const Schematic before = s;
    const Edit move = applied(s, editing::move_items(s, lib, {r}, {4, 2}));
    CHECK((s.component(r)->position == Point{4, 2}));
    const auto& w1 = s.wire(straight)->points;
    const auto& w2 = s.wire(bent)->points;
    CHECK(w1.front() == pin(lib, s, r, "1"));
    CHECK(w1.back() == (Point{0, -10})); // far end stays
    CHECK(is_orthogonal(w1));
    CHECK(w2.front() == pin(lib, s, r, "2"));
    CHECK(w2.back() == (Point{12, 8}));
    CHECK(is_orthogonal(w2));
    move.revert(s);
    CHECK(s == before);
}

TEST_CASE("moving everything translates wires unchanged")
{
    const auto lib = test_library();
    Schematic s;
    Id r = 0, w = 0;
    applied(s, editing::add_component(s, *lib.find("resistor"), {0, 0}, {}, &r));
    applied(s, editing::add_wire(s, {{0, 3}, {0, 6}, {5, 6}}, &w));
    applied(s, editing::move_items(s, lib, {r, w}, {1, 1}));
    CHECK((s.wire(w)->points == std::vector<Point>{{1, 4}, {1, 7}, {6, 7}}));
}

TEST_CASE("a wire between two moved components moves along")
{
    const auto lib = test_library();
    Schematic s;
    Id a = 0, b = 0, w = 0;
    applied(s, editing::add_component(s, *lib.find("resistor"), {0, 0}, {}, &a));
    applied(s, editing::add_component(s, *lib.find("resistor"), {0, 10}, {}, &b));
    applied(s, editing::add_wire(s, {{0, 3}, {0, 7}}, &w));
    applied(s, editing::move_items(s, lib, {a, b}, {3, 0}));
    CHECK((s.wire(w)->points == std::vector<Point>{{3, 3}, {3, 7}}));
}

TEST_CASE("rotation and mirroring keep wires attached")
{
    const auto lib = test_library();
    Schematic s;
    Id r = 0, w = 0;
    applied(s, editing::add_component(s, *lib.find("resistor"), {0, 0}, {}, &r));
    applied(s, editing::add_wire(s, {{0, 3}, {0, 9}}, &w));
    applied(s, editing::rotate_items(s, lib, {r}));
    CHECK(s.component(r)->orientation.rotation == 90);
    CHECK((pin(lib, s, r, "2") == Point{-3, 0}));
    CHECK(s.wire(w)->points.front() == pin(lib, s, r, "2"));
    CHECK(is_orthogonal(s.wire(w)->points));
    applied(s, editing::mirror_items(s, lib, {r}));
    CHECK(s.component(r)->orientation.mirrored);
    CHECK((pin(lib, s, r, "2") == Point{3, 0}));
    CHECK(s.wire(w)->points.front() == pin(lib, s, r, "2"));
    CHECK(s.component(r)->orientation.rotation == 270); // mirror after 90 degrees
    applied(s, editing::rotate_items(s, lib, {r}));
    CHECK(s.component(r)->orientation.rotation == 0);
}

TEST_CASE("a group rotates about its center on the grid")
{
    const auto lib = test_library();
    Schematic s;
    Id l = 0;
    applied(s, editing::add_label(s, {0, 0}, "A", &l));
    Id m = 0;
    applied(s, editing::add_label(s, {4, 0}, "B", &m));
    applied(s, editing::rotate_items(s, lib, {l, m}));
    CHECK((s.label(l)->position == Point{2, -2}));
    CHECK((s.label(m)->position == Point{2, 2}));
}

TEST_CASE("delete, parameters and designators are undoable")
{
    const auto lib = test_library();
    Schematic s;
    Id r = 0;
    applied(s, editing::add_component(s, *lib.find("resistor"), {0, 0}, {}, &r));
    const auto set = applied(s, editing::set_parameter(s, r, "R", "4k7"));
    CHECK(s.component(r)->parameters.at("R") == "4k7");
    const auto rename = applied(s, editing::set_designator(s, r, "Rload"));
    CHECK(s.component(r)->designator == "Rload");
    const auto text = applied(s, editing::move_text(s, r, true, {4, 0}));
    CHECK((s.component(r)->value_offset == Point{4, 0}));
    const auto removed = applied(s, editing::delete_items(s, {r}));
    CHECK(s.components().empty());
    removed.revert(s);
    text.revert(s);
    rename.revert(s);
    set.revert(s);
    CHECK(s.component(r)->designator == "R1");
    CHECK(s.component(r)->parameters.at("R") == "1k");
    CHECK(!s.component(r)->value_offset.has_value());
    CHECK(editing::set_parameter(s, 999, "R", "1").empty());
}

TEST_CASE("text notes are placed, edited, moved, pasted and deleted")
{
    const auto lib = test_library();
    Schematic s;
    CHECK(editing::add_note(s, {0, 0}, "  \n ").empty()); // blank: nothing to place
    Id n = 0;
    const auto placed = applied(s, editing::add_note(s, {2, -4}, "Close SW1.\nThen watch the LED.", &n));
    REQUIRE(s.note(n) != nullptr);
    CHECK(s.note(n)->text == "Close SW1.\nThen watch the LED.");
    const auto changed = applied(s, editing::set_note_text(s, n, "Press the button."));
    CHECK(s.note(n)->text == "Press the button.");
    CHECK(editing::set_note_text(s, n, "Press the button.").empty()); // unchanged
    applied(s, editing::move_items(s, lib, {n}, {3, 1}));
    CHECK((s.note(n)->position == Point{5, -3}));
    std::set<Id> created;
    applied(s, editing::paste_items(s, lib, editing::copy_items(s, {n}), {0, 10}, &created));
    REQUIRE(created.size() == 1);
    CHECK((s.note(*created.begin())->position == Point{5, 7}));
    const auto cleared = applied(s, editing::set_note_text(s, n, " "));
    CHECK(s.note(n) == nullptr); // a blank text deletes the note
    cleared.revert(s);
    CHECK(s.note(n)->text == "Press the button.");
    applied(s, editing::delete_items(s, {n}));
    CHECK(s.notes().size() == 1);
    (void)placed;
    (void)changed;
}

TEST_CASE("copy and paste create new ids and designators")
{
    const auto lib = test_library();
    Schematic s;
    Id r = 0, w = 0;
    applied(s, editing::add_component(s, *lib.find("resistor"), {0, 0}, {}, &r));
    applied(s, editing::add_wire(s, {{0, 3}, {0, 6}}, &w));
    Probe probe;
    probe.kind = ProbeKind::Current;
    probe.target = {r, "1", {}};
    Id p = 0;
    applied(s, editing::add_probe(s, probe, &p));
    const std::string clip = editing::copy_items(s, {r, w, p});
    std::set<Id> created;
    applied(s, editing::paste_items(s, lib, clip, {10, 0}, &created));
    CHECK(created.size() == 3);
    CHECK(s.components().size() == 2);
    const auto& copy = s.components().back();
    CHECK(copy.id != r);
    CHECK(copy.designator == "R2");
    CHECK((copy.position == Point{10, 0}));
    CHECK((s.wires().back().points.front() == Point{10, 3}));
    CHECK(s.probes().back().target.item == copy.id); // probe follows the copy
    CHECK(editing::paste_items(s, lib, "not a clipboard", {}).empty());
}
