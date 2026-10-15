// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "test_support.hpp"

#include "library/component_library.hpp"
#include "library/spice_template.hpp"

#include <cmath>

using namespace simnodus;
using library::ComponentLibrary;

namespace {

const char* minimal = R"({
  "type": "t", "name": "T", "category": "Passive", "prefix": "T",
  "symbol": {"graphics": [{"line": [0, -3, 0, 3]}],
             "pins": [{"name": "1", "at": [0, -3], "dir": "up"}, {"name": "2", "at": [0, 3], "dir": "down"}]},
  "parameters": [{"name": "R", "unit": "Ohm", "default": "1k"}],
  "spice": ["R_{ref} {node:1} {node:2} {R}"]
})";

std::string with(const std::string& from, const std::string& to)
{
    std::string text = minimal;
    const auto at = text.find(from);
    return at == std::string::npos ? text : text.replace(at, from.size(), to);
}

} // namespace

TEST_CASE("component set A loads without errors")
{
    ComponentLibrary library;
    const auto messages = library.load_directory(SIMNODUS_TEST_LIBRARY_DIR);
    for (const auto& m : messages) {
        std::printf("  %s: %s\n", m.file.string().c_str(), m.text.c_str());
    }
    CHECK(messages.empty());
    for (const char* type : {"resistor", "capacitor", "inductor", "potentiometer", "vdc", "idc", "battery", "vpulse",
             "vsine", "ground", "spst", "spdt", "button_no", "button_nc", "diode", "led", "npn", "nmos", "voltmeter",
             "ammeter"}) {
        if (!library.find(type)) {
            std::printf("  missing %s\n", type);
        }
        CHECK(library.find(type) != nullptr);
    }
    const auto categories = library.categories();
    REQUIRE(!categories.empty());
    CHECK(categories.front() == "Passive");
    const auto* resistor = library.find("resistor");
    REQUIRE(resistor != nullptr);
    CHECK(resistor->prefix == "R");
    CHECK(resistor->pins.size() == 2);
    CHECK(resistor->default_parameters().at("R") == "1k");
    CHECK(library.find("ground")->ties_net == "0");
    CHECK(library.find("spst")->interactive.has_value());
    CHECK(library.find("spst")->state_graphics.count("1") == 1);
    CHECK(library.find("led")->parameter("color")->choices.size() == 5);
}

TEST_CASE("pin positions follow placement and orientation")
{
    ComponentLibrary library;
    REQUIRE(!library.add_from_json(minimal));
    const auto* d = library.find("t");
    REQUIRE(d != nullptr);
    model::Component c;
    c.position = {10, 20};
    CHECK((library::pin_position(*d, d->pins[0], c) == model::Point{10, 17}));
    c.orientation.rotation = 90;
    CHECK((library::pin_position(*d, d->pins[0], c) == model::Point{13, 20}));
}

TEST_CASE("invalid definitions are rejected with a reason")
{
    ComponentLibrary library;
    CHECK(library.add_from_json("{ not json").has_value());
    CHECK(library.add_from_json(with("\"prefix\": \"T\",", "")).has_value());
    CHECK(library.add_from_json(with("{node:2}", "{node:9}")).has_value());
    CHECK(library.add_from_json(with("{R}", "{Q}")).has_value());
    CHECK(library.add_from_json(with("{R}", "{R")).has_value());
    CHECK(library.add_from_json(with("\"default\": \"1k\"", "\"default\": \"abc\"")).has_value());
    CHECK(library.add_from_json(with("[0, 3], \"dir\": \"down\"", "[0, 2.5], \"dir\": \"down\"")).has_value());
    CHECK(library.add_from_json(with("[0, 3], \"dir\": \"down\"", "[0, -3], \"dir\": \"down\"")).has_value());
    CHECK(library.add_from_json(with("\"dir\": \"up\"", "\"dir\": \"sideways\"")).has_value());
    CHECK(library.add_from_json(with("{R}", "{control}")).has_value());
    CHECK(library.add_from_json(with("{R}", "{= R * X}")).has_value());
    CHECK(library.empty());
    CHECK(!library.add_from_json(minimal));
    const auto duplicate = library.add_from_json(minimal);
    REQUIRE(duplicate.has_value());
    CHECK(duplicate->find("duplicate") != std::string::npos);
}

TEST_CASE("a missing library folder is reported")
{
    ComponentLibrary library;
    const auto messages = library.load_directory("no/such/folder");
    CHECK(messages.size() == 1);
    CHECK(library.empty());
}

TEST_CASE("templates are tokenized")
{
    std::string error;
    const auto tokens = library::tokenize_template("R_{ref} {node:1} {node: 2} {R} {= 1/f} {control}", error);
    REQUIRE(tokens.has_value());
    using K = library::TemplateToken::Kind;
    std::vector<K> kinds;
    for (const auto& t : *tokens) {
        if (t.kind != K::Literal) {
            kinds.push_back(t.kind);
        }
    }
    CHECK((kinds == std::vector<K>{K::Ref, K::Node, K::Node, K::Parameter, K::Expression, K::Control}));
    CHECK((*tokens)[0].text == "R_");
    CHECK((*tokens)[5].text == "2"); // "R_", ref, " ", node 1, " ", node 2
    CHECK(!library::tokenize_template("{}", error));
    CHECK(!library::tokenize_template("a}", error));
    CHECK(!library::tokenize_template("{a b}", error));
}

TEST_CASE("template expressions evaluate parameters")
{
    const library::ValueLookup lookup = [](std::string_view name) -> std::optional<double> {
        if (name == "f") {
            return 1000.0;
        }
        if (name == "duty") {
            return 25.0;
        }
        return std::nullopt;
    };
    std::string error;
    auto v = library::evaluate_expression("duty/100/f", lookup, error);
    CHECK(v && std::abs(*v - 2.5e-4) < 1e-15);
    v = library::evaluate_expression("max(duty/100/f - 1e-3, 1e-6)", lookup, error);
    CHECK(v && std::abs(*v - 1e-6) < 1e-18);
    v = library::evaluate_expression("-(2 + 3) * 2", lookup, error);
    CHECK(v && *v == -10.0);
    CHECK(!library::evaluate_expression("1/0", lookup, error));
    CHECK(error == "division by zero");
    CHECK(!library::evaluate_expression("g * 2", lookup, error));
    CHECK(!library::evaluate_expression("2 +", lookup, error));
    CHECK(!library::evaluate_expression("(2", lookup, error));
    CHECK((library::expression_names("max(duty/100/f - 1e-3, rise)") == std::vector<std::string>{"duty", "f", "rise"}));
}

TEST_CASE("the Blue Pill board loads with its real pins and board profile")
{
    ComponentLibrary library;
    library.load_directory(SIMNODUS_TEST_LIBRARY_DIR);
    const auto* d = library.find("bluepill");
    REQUIRE(d != nullptr);
    CHECK(d->category == "Microcontrollers");
    CHECK(d->prefix == "U");
    CHECK(d->pins.size() == 40);
    int gpio = 0;
    for (const auto& p : d->pins) {
        gpio += p.name.size() >= 3 && p.name[0] == 'P' ? 1 : 0;
    }
    CHECK(gpio == 32);
    for (const char* name : {"PA0", "PA15", "PB12", "PC13", "PC15", "NRST", "5V", "VB", "G1", "3V3_2"}) {
        CHECK(d->pin(name) != nullptr);
    }
    REQUIRE(d->mcu.has_value());
    CHECK(d->mcu->platform == "stm32f103c8.repl");
    REQUIRE(d->mcu->led.has_value());
    CHECK(d->mcu->led->pin == "PC13");
    CHECK(d->mcu->led->active_low);
    CHECK(d->mcu->reset_button.has_value());
    const auto* firmware = d->parameter(d->mcu->firmware_parameter);
    REQUIRE(firmware != nullptr);
    CHECK(firmware->kind == library::Parameter::Kind::File);
    CHECK(firmware->filter == "*.elf");
}

TEST_CASE("an mcu section needs a file parameter for the firmware")
{
    ComponentLibrary library;
    const auto error = library.add_from_json(R"({"type": "board", "name": "Board", "category": "Microcontrollers",
        "prefix": "U", "symbol": {"graphics": [], "pins": [{"name": "PA0", "at": [0, 0], "dir": "up"}]},
        "parameters": [{"name": "firmware", "default": "1"}],
        "mcu": {"platform": "x.repl", "firmware_parameter": "firmware"},
        "spice": ["R_{ref} {node:PA0} 0 1k"]})");
    REQUIRE(error.has_value());
    CHECK(error->find("file parameter") != std::string::npos);
}
