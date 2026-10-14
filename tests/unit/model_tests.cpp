// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "test_support.hpp"

#include "model/edit.hpp"
#include "model/engineering.hpp"
#include "model/geometry.hpp"
#include "model/schematic.hpp"

#include <cmath>

using namespace simnodus::model;

namespace {

bool near(std::optional<double> value, double expected)
{
    return value && std::abs(*value - expected) <= std::abs(expected) * 1e-12 + 1e-30;
}

Component resistor(Id id, const std::string& designator, Point at)
{
    Component c;
    c.id = id;
    c.type = "resistor";
    c.designator = designator;
    c.position = at;
    c.parameters["R"] = "1k";
    return c;
}

} // namespace

TEST_CASE("engineering notation accepts the usual spellings")
{
    CHECK(near(parse_engineering("1000"), 1000));
    CHECK(near(parse_engineering("4.7k"), 4700));
    CHECK(near(parse_engineering("4k7"), 4700));
    CHECK(near(parse_engineering("100n"), 100e-9));
    CHECK(near(parse_engineering("2.2u"), 2.2e-6));
    CHECK(near(parse_engineering("2u2"), 2.2e-6));
    CHECK(near(parse_engineering("2.2\xC2\xB5"), 2.2e-6));
    CHECK(near(parse_engineering("1meg"), 1e6));
    CHECK(near(parse_engineering("1M"), 1e6));
    CHECK(near(parse_engineering("1m"), 1e-3));
    CHECK(near(parse_engineering("1e-3"), 1e-3));
    CHECK(near(parse_engineering(" -2.5 "), -2.5));
    CHECK(near(parse_engineering("+3"), 3));
    CHECK(near(parse_engineering("10G"), 10e9));
}

TEST_CASE("engineering notation accepts units and rejects nonsense")
{
    CHECK(near(parse_engineering("4.7kOhm", "Ohm"), 4700));
    CHECK(near(parse_engineering("4.7k ohm", "Ohm"), 4700));
    CHECK(near(parse_engineering("10k\xCE\xA9", "Ohm"), 10e3));
    CHECK(near(parse_engineering("100R", "Ohm"), 100));
    CHECK(near(parse_engineering("100nF", "F"), 100e-9));
    CHECK(near(parse_engineering("1f", "F"), 1e-15)); // femto, not one farad
    CHECK(near(parse_engineering("5V", "V"), 5));
    CHECK(!parse_engineering(""));
    CHECK(!parse_engineering("abc"));
    CHECK(!parse_engineering("4.7.1k"));
    CHECK(!parse_engineering("4k7x"));
    CHECK(!parse_engineering("1.5k7"));
    CHECK(!parse_engineering("k"));
    CHECK(!parse_engineering("inf"));
    CHECK(!parse_engineering("5V", "A"));
}

TEST_CASE("engineering values are formatted with a multiplier")
{
    CHECK(format_engineering(4700) == "4.7k");
    CHECK(format_engineering(1e-7) == "100n");
    CHECK(format_engineering(0) == "0");
    CHECK(format_engineering(1e6) == "1M");
    CHECK(format_engineering(-0.0033) == "-3.3m");
    CHECK(format_engineering(999.99999999999) == "1k");
    CHECK(spice_number(4700) == "4700");
    CHECK(spice_number(1e-7) == "1e-07");
    CHECK(spice_number(0.5) == "0.5");
}

TEST_CASE("orientation transforms follow the screen convention")
{
    const Point p{0, -3}; // pin above the origin
    CHECK((orient(p, {90, false}) == Point{3, 0}));    // clockwise: to the right
    CHECK((orient(p, {180, false}) == Point{0, 3}));
    CHECK((orient(p, {270, false}) == Point{-3, 0}));
    CHECK((orient(Point{2, 1}, {0, true}) == Point{-2, 1}));
    CHECK((orient(Point{2, 1}, {90, true}) == Point{-1, -2}));
    CHECK((transform(p, Point{10, 10}, {90, false}) == Point{13, 10}));
    CHECK(normalized_rotation(-90) == 270);
    CHECK(normalized_rotation(450) == 90);
    CHECK((rotate_cw(Point{1, 0}, Point{0, 0}) == Point{0, 1}));
    CHECK(on_segment(Point{2, 0}, Point{0, 0}, Point{5, 0}));
    CHECK(!on_segment(Point{6, 0}, Point{0, 0}, Point{5, 0}));
    CHECK(on_segment(Point{0, 3}, Point{0, 5}, Point{0, 1}));
}

TEST_CASE("schematic keeps elements by stable id")
{
    Schematic s;
    const Id a = s.allocate_id();
    const Id b = s.allocate_id();
    s.put(resistor(b, "R2", {0, 0}));
    s.put(resistor(a, "R1", {5, 0}));
    REQUIRE(s.components().size() == 2);
    CHECK(s.components()[0].id == a); // ordered by id
    CHECK(s.component(b)->designator == "R2");
    CHECK(s.component_by_designator("R1")->id == a);
    s.put(Wire{s.allocate_id(), {{0, 0}, {5, 0}}});
    CHECK(s.wires().size() == 1);
    s.erase(a);
    CHECK(!s.component(a));
    CHECK(s.allocate_id() > b); // never reused
    // Putting an element with an id beyond the counter advances it.
    s.put(NetLabel{100, {1, 1}, "VCC"});
    CHECK(s.allocate_id() == 101);
    s.set_next_id(5);
    CHECK(s.next_id() >= 101);
}

TEST_CASE("designators are numbered per prefix")
{
    Schematic s;
    CHECK(next_designator(s, "R") == "R1");
    s.put(resistor(s.allocate_id(), "R1", {}));
    s.put(resistor(s.allocate_id(), "R3", {}));
    s.put(resistor(s.allocate_id(), "RV1", {}));
    CHECK(next_designator(s, "R") == "R2");
    CHECK(next_designator(s, "RV") == "RV2");
    CHECK(next_designator(s, "C") == "C1");
}

TEST_CASE("an edit applies and reverts exactly")
{
    Schematic before;
    before.put(resistor(before.allocate_id(), "R1", {0, 0}));
    before.put(Wire{before.allocate_id(), {{0, 3}, {0, 10}}});
    Schematic after = before;
    Component moved = *after.component(1);
    moved.position = {4, 0};
    after.put(moved);
    after.erase(2);
    after.put(NetLabel{after.allocate_id(), {3, 3}, "OUT"});
    SimulationSettings settings;
    settings.speed = 0.1;
    after.set_settings(settings);

    const Edit edit = make_edit(before, after, "test");
    CHECK(edit.description() == "test");
    CHECK(edit.changes().size() == 3);
    Schematic doc = before;
    edit.apply(doc);
    CHECK(doc.components() == after.components());
    CHECK(doc.wires().empty());
    CHECK(doc.labels() == after.labels());
    CHECK(doc.settings().speed == 0.1);
    edit.revert(doc);
    CHECK(doc.components() == before.components());
    CHECK(doc.wires() == before.wires());
    CHECK(doc.labels().empty());
    CHECK(doc.settings() == before.settings());
    CHECK(make_edit(before, before, "none").empty());
}
