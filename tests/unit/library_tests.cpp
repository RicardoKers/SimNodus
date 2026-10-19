// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "test_support.hpp"

#include "library/component_library.hpp"
#include "library/spice_model.hpp"
#include "library/spice_template.hpp"

#include <algorithm>
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
        "mcu": {"firmware_parameter": "firmware"},
        "spice": ["R_{ref} {node:PA0} 0 1k"]})");
    REQUIRE(error.has_value());
    CHECK(error->find("file parameter") != std::string::npos);
}

TEST_CASE("SPICE model files: .model statements are read, the rest is skipped")
{
    const std::string text = "* Models pasted from a vendor library\n"
                             "* 2N2222A: general purpose NPN\n"
                             ".MODEL 2N2222A npn (IS=14.34f BF=255.9 ; end-of-line comment\n"
                             "+ VAF=74.03 IKF=.2847)\n"
                             "\n"
                             ".model D1N4148 D IS=2.52n RS=.568\n"
                             ".subckt OPAMP 1 2 3\n"
                             ".model INNER NPN(BF=50)\n"
                             ".ends OPAMP\n"
                             ".model JF1 NJF(VTO=-2)\n"
                             ".model BAD NPN(BF={beta} IS=1f)\n"
                             "R1 1 2 1k\n"
                             ".control\n";
    std::vector<library::ModelIssue> issues;
    const auto models = library::parse_spice_models(text, &issues);
    REQUIRE(models.size() == 3);
    CHECK(models[0].name == "2N2222A");
    CHECK(models[0].type == "NPN");
    CHECK(models[0].parameters == "IS=14.34f BF=255.9 VAF=74.03 IKF=.2847");
    CHECK(models[0].description == "2N2222A: general purpose NPN");
    CHECK(models[0].card("BM_X") == ".model BM_X NPN(IS=14.34f BF=255.9 VAF=74.03 IKF=.2847)");
    CHECK(models[1].name == "D1N4148");
    CHECK(models[1].parameters == "IS=2.52n RS=.568");
    CHECK(models[1].description.empty());
    // The JFET is skipped (no subcircuit uses it); BAD keeps IS but not the expression.
    CHECK(models[2].name == "BAD");
    CHECK(models[2].parameters == "IS=1f");
    REQUIRE(issues.size() == 2);
    CHECK(issues[0].line == 11);
    CHECK(issues[0].text == "left out BF={beta} (not a number)");
    CHECK(issues[1].model == "JF1");
    CHECK(issues[1].text.find("NJF is not supported") != std::string::npos);
    // Only the statement itself is kept: text after the parameters is dropped.
    CHECK(library::parse_spice_model(".model X NPN(BF=100) .control")->card() == ".model X NPN(BF=100)");
    CHECK(!library::parse_spice_model(".model X$Y NPN(BF=100)").has_value());
}

TEST_CASE("SPICE models: parameters ngspice cannot use are left out and reported")
{
    // As in vendor and LTspice files: descriptive fields, a wrong value, an unknown name.
    const auto m = library::parse_spice_model(
        ".model Q1 NPN(IS=1E-14 Bf=300 mfg=NXP Vceo=40 Icrating=200m FOO=3 Rb=abc VAF = 100 Iave=\"1 A\")");
    REQUIRE(m.has_value());
    CHECK(m->parameters == "IS=1E-14 Bf=300 VAF=100");
    CHECK((m->dropped == std::vector<std::string>{"FOO=3 (not a NPN parameter)", "Rb=abc (not a number)"}));
    // A diode with LTspice fields keeps what ngspice knows.
    const auto d = library::parse_spice_model(".model 1N4007 D(Is=7n Rs=.04 N=1.8 Cjo=15p Iave=1 Vpk=1000 mfg=Diodes type=silicon)");
    REQUIRE(d.has_value());
    CHECK(d->parameters == "Is=7n Rs=.04 N=1.8 Cjo=15p");
    CHECK(d->dropped.empty());
    // MOSFET levels above 3 have too many parameters to list: only values are checked.
    const auto bsim = library::parse_spice_model(".model M1 NMOS(LEVEL=49 VERSION=3.3 K1=0.5 TOXM=x)");
    REQUIRE(bsim.has_value());
    CHECK(bsim->parameters == "LEVEL=49 VERSION=3.3 K1=0.5");
    CHECK((bsim->dropped == std::vector<std::string>{"TOXM=x (not a number)"}));
    std::string why;
    CHECK(!library::parse_spice_model(".model X NPN(BF=100", &why).has_value());
    CHECK(why == "a parenthesis is not closed");
}

TEST_CASE("diode parts offer the models of their kind")
{
    ComponentLibrary library;
    library.load_directory(SIMNODUS_TEST_LIBRARY_DIR);
    auto offered = [&library](const char* type) {
        std::vector<std::string> names;
        const auto* p = library.find(type)->parameter("model");
        for (const auto& m : library.models()) {
            if (library::model_fits(*p, m)) {
                names.push_back(m.name);
            }
        }
        return names;
    };
    const auto diode = offered("diode");
    const auto schottky = offered("schottky");
    const auto germanium = offered("germanium");
    CHECK(std::find(diode.begin(), diode.end(), "1N4007") != diode.end());
    CHECK(std::find(diode.begin(), diode.end(), "1N5819") == diode.end());
    CHECK((schottky == std::vector<std::string>{"1N5819", "BAT54", "1N5822"}));
    CHECK((germanium == std::vector<std::string>{"1N34A", "1N60"}));
    // A vendor model's "type=" decides where it appears; one without it is a plain diode.
    const auto ss = library::parse_spice_model(".model SS24 D(Is=.4mA N=2.1 mfg=Fairchild type=Schottky)");
    REQUIRE(ss.has_value());
    CHECK(ss->kind == "schottky");
    CHECK(library::model_fits(*library.find("schottky")->parameter("model"), *ss));
    CHECK(!library::model_fits(*library.find("diode")->parameter("model"), *ss));
    const auto plain = library::parse_spice_model(".model D1 D(IS=1n)");
    CHECK(library::model_fits(*library.find("diode")->parameter("model"), *plain));
}

TEST_CASE("the built-in models cover the semiconductor parts")
{
    ComponentLibrary library;
    const auto messages = library.load_directory(SIMNODUS_TEST_LIBRARY_DIR);
    CHECK(messages.empty());
    for (const char* type : {"npn", "pnp", "diode", "nmos", "pmos"}) {
        const auto* d = library.find(type);
        REQUIRE(d != nullptr);
        const auto* p = d->parameter("model");
        REQUIRE(p != nullptr);
        CHECK(p->kind == library::Parameter::Kind::Model);
        CHECK(library.model(p->model_type, "Generic") != nullptr);
    }
    CHECK(library.model("NPN", "2N2222A") != nullptr);
    CHECK(library.model("D", "1N5819") != nullptr);
    CHECK(library.model("NPN", "1N5819") == nullptr); // names are looked up per type
    CHECK(library.models().size() >= 20);
}

TEST_CASE("the potentiometer's wiper moves along its track")
{
    ComponentLibrary library;
    library.load_directory(SIMNODUS_TEST_LIBRARY_DIR);
    const auto* d = library.find("potentiometer");
    REQUIRE(d != nullptr && d->interactive.has_value() && d->interactive->wiper.has_value());
    const auto& w = *d->interactive->wiper;
    CHECK(w.pin == "W");
    CHECK(w.start.y < w.end.y); // 0 % at pin 1 (top), 100 % at pin 2
    CHECK(d->pin("1")->at.y < d->pin("2")->at.y);
}

TEST_CASE("vendor subcircuits are read with the models and blocks they use")
{
    const std::string text = "* Op-amp macro model\n"
                             ".SUBCKT AMP/X 1 2 99 50 28 PARAMS: GAIN=1e5\n"
                             "* connections: in+ in- v+ v- out\n"
                             "Q1 5 2 4 QX\n"
                             "E1 28 0 POLY(1) 1 2 0 {GAIN}\n"
                             "XB 28 50 BUF\n"
                             ".MODEL INNER D(IS=1f)\n"
                             ".ENDS\n"
                             ".SUBCKT BUF 1 2\n"
                             "R1 1 2 1k\n"
                             "D1 1 2 DLIM\n"
                             ".ENDS BUF\n"
                             ".SUBCKT BAD 1 2\n"
                             ".include secrets.lib\n"
                             "R1 1 2 1k\n"
                             ".ENDS\n"
                             ".SUBCKT USESBAD 1 2\n"
                             "X1 1 2 BAD\n"
                             ".ENDS\n"
                             ".MODEL QX PNP(BF=200)\n"
                             ".MODEL DLIM D(IS=1e-15)\n"
                             ".MODEL UNUSED D(IS=1n)\n"
                             ".MODEL RT RES(TC1=1m)\n";
    std::vector<library::ModelIssue> issues;
    const auto lib = library::parse_spice_library(text, &issues);
    REQUIRE(lib.subcircuits.size() == 2); // AMP/X and BUF; BAD and USESBAD are skipped
    const auto& amp = lib.subcircuits[0];
    CHECK(amp.name == "AMP/X");
    CHECK((amp.ports == std::vector<std::string>{"1", "2", "99", "50", "28"}));
    CHECK(amp.description == "Op-amp macro model\nconnections: in+ in- v+ v- out");
    // Self-contained: the models and the block it uses, then its own block last.
    CHECK(amp.text.find(".MODEL QX PNP(BF=200)") != std::string::npos);
    CHECK(amp.text.find(".MODEL DLIM D(IS=1e-15)") != std::string::npos);
    CHECK(amp.text.find(".SUBCKT BUF 1 2") < amp.text.find(".SUBCKT AMP/X"));
    CHECK(amp.text.find("UNUSED") == std::string::npos);
    CHECK(amp.text.find(".include") == std::string::npos);
    CHECK(lib.subcircuits[1].name == "BUF");
    // The kept text reads back as the same subcircuit.
    const auto kept = library::parse_subcircuit_text(amp.text);
    REQUIRE(kept.has_value());
    CHECK(kept->name == "AMP/X");
    CHECK(kept->ports.size() == 5);
    bool include_reported = false;
    bool uses_bad_reported = false;
    for (const auto& issue : issues) {
        include_reported = include_reported || (issue.model == "BAD" && issue.text.find(".INCLUDE") != std::string::npos);
        uses_bad_reported = uses_bad_reported || (issue.model == "USESBAD" && issue.text.find("BAD") != std::string::npos);
    }
    CHECK(include_reported);
    CHECK(uses_bad_reported);
    std::string why;
    CHECK(!library::parse_subcircuit_text(".SUBCKT S 1 2\n.control\nshell rm x\n.endc\n.ENDS\n", &why).has_value());
    CHECK(why.find(".CONTROL") != std::string::npos);
}
