// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// Component library (PRJ-04): one JSON file per component type, describing
// the symbol (graphics and pins, in grid units), the parameters, the SPICE
// template and optional interactive behavior. Nothing in C++ is specific to
// one component type; a future Symbol Editor writes the same files.
// Format: docs/architecture/COMPONENT_LIBRARY.md.
#pragma once

#include "model/geometry.hpp"
#include "model/schematic.hpp"
#include "spice_model.hpp"

#include <cstdint>
#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace simnodus::library {

enum class PinDirection { Left, Right, Up, Down };

struct Pin {
    std::string name;
    model::Point at; // grid units, relative to the symbol origin
    PinDirection direction = PinDirection::Up;
    bool optional = false; // may stay unconnected without a warning (an unused Q-bar or clear input)
};

struct Graphic {
    enum class Kind { Line, Rect, Circle, Arc, Polyline, Polygon, Text };
    Kind kind = Kind::Line;
    std::vector<model::PointF> points; // line: 2; rect: top-left + size; circle/arc: center
    double radius = 0.0;
    double start_degrees = 0.0; // arc: counter-clockwise from 3 o'clock, as drawn on screen
    double span_degrees = 0.0;
    bool filled = false;
    std::string text;
    double text_size = 1.0; // grid units
};

struct Parameter {
    // Number (engineering notation), Choice (one of `choices`), File (a path,
    // stored relative to the project when possible; `filter` like "*.elf"),
    // Model (the name of a SPICE model of type `model_type`, PRJ-07) or Text
    // (any words, a block port's name).
    enum class Kind { Number, Choice, File, Model, Text };
    Kind kind = Kind::Number;
    std::string name;
    std::string label;
    std::string unit; // "Ohm", "F", "H", "V", "A", "Hz", "s", "%", "deg" or empty
    std::string default_value;
    std::optional<double> min;
    std::optional<double> max;
    std::vector<std::string> choices; // non-empty: the value is one of these words
    std::string description;
    std::string filter;
    std::string symbol;     // shown before the value on the schematic ("β" gives "β=100"); empty: the value alone
    std::string model_type; // Model: "NPN", "PNP", "D", "NMOS" or "PMOS"; empty: subcircuits only
    // Model: the part's pins in the usual port order of vendor subcircuits
    // ("IN+", "IN-", "V+", "V-", "OUT"); empty: no subcircuits (ADR 0097).
    std::vector<std::string> subcircuit;
    // Model: the "type=" values of the models this part offers ("schottky"); an
    // empty string stands for models without one. Empty list: any model of the type.
    std::vector<std::string> model_kinds;
    // Choice: the part's orientation for some values (a block port's side turns
    // the port so that its wire attaches on that side).
    std::map<std::string, model::Orientation> orientations;
};

// Microcontroller board (MCU-01, INT-06): runs firmware in SimNodus's own
// STM32F103 emulator (ADR 0089, 0094). Pins named like "PA0" are GPIO pins of
// port A; the others are power or control pins.
struct McuBoard {
    std::string firmware_parameter;       // a File parameter
    std::string ground_pin = "G1";        // reference of the pin voltages
    // Electrical GPIO model (MCU-03): driven outputs through a resistance to the
    // supply or ground, internal pull resistors of inputs.
    struct PinModel {
        double supply = 3.3;              // V, relative to the board ground pin
        double output_resistance = 40.0;  // Ohm
        double pull_resistance = 40000.0; // Ohm
        double input_high = 1.83;         // V: an input reads 1 above this (Schmitt, rising)
        double input_low = 1.16;          // V: and 0 below this (falling)
    };
    PinModel pin_model;
    struct Led {
        std::string pin;         // GPIO driving the on-board LED
        bool active_low = false; // lit while the pin is low
        model::PointF at;        // symbol coordinates
        double radius = 0.6;
    };
    std::optional<Led> led;
    std::optional<std::vector<model::PointF>> reset_button; // top-left and size, symbol coordinates
    struct Uart {
        std::string name;  // peripheral ("usart1")
        std::string label; // terminal text ("USART1 (PA9 TX, PA10 RX)")
        std::string tx_pin; // "PA9": held high while the transmitter is on; empty if unknown
        std::string rx_pin; // "PA10": a board's TX on the same net sends it bytes (SN-175)
    };
    std::vector<Uart> uarts; // serial ports for the terminal (MCU-05)
};

// Interactive control (INT-02). The netlist generator creates an external
// voltage source on node "{control}" whose value is the control state:
// 0/1 for toggle and button, 0..1 for a potentiometer position.
struct Interactive {
    enum class Kind { Toggle, Button, Potentiometer };
    Kind kind = Kind::Toggle;
    std::string parameter; // parameter holding the initial state ("closed", "position")
    // Potentiometer (UI-18): the wiper arrow's tip moves along this track, from
    // the first point (0 %) to the second (100 %), and joins the wiper pin.
    struct Wiper {
        model::PointF start;
        model::PointF end;
        std::string pin;
    };
    std::optional<Wiper> wiper;
};

// Value shown on the symbol while running (INT-03).
struct Readout {
    enum class Kind { Voltage, Current, Node };
    Kind kind = Kind::Voltage;
    std::vector<std::string> pins; // voltage: + and - pin
    std::string element;           // current: template element name, e.g. "V_{ref}"; node: internal node, "w_{ref}"
    double scale = 1.0;            // node: shown value = scale x node voltage
    std::string unit;              // node: unit of the shown value
    // Current readouts may make the symbol glow: full brightness at glow_full
    // amperes, or at the value of parameter glow_full_parameter; the color is
    // glow_color ("#rrggbb") or the value of parameter glow_color_parameter.
    double glow_full = 0.0;
    std::string glow_full_parameter;
    std::string glow_color;
    std::string glow_color_parameter;
    bool glows() const { return glow_full > 0 || !glow_full_parameter.empty(); }
    // Current readouts may switch the symbol's state graphics instead (a relay's
    // contact): state "1" above `switch_on` amperes, back to "0" below
    // `switch_off`; both are expressions of the parameters ("0.42 * 0.36 / Vcoil").
    std::string switch_on;
    std::string switch_off;
    bool switches() const { return !switch_on.empty(); }
};

struct ComponentDefinition {
    std::string type;
    std::string name;
    std::string category;
    std::string prefix;
    std::string description;
    std::vector<Graphic> graphics;
    // Extra graphics drawn for a control state ("0", "1"); toggles and buttons.
    std::map<std::string, std::vector<Graphic>> state_graphics;
    std::vector<Pin> pins;
    model::Point designator_offset{2, -1};
    model::Point value_offset{2, 1};
    bool show_designator = true;
    std::string value_parameter; // shown next to the symbol
    std::vector<Parameter> parameters;
    std::vector<std::string> spice;  // template lines
    std::vector<std::string> models; // .model lines, emitted once per netlist
    std::string ties_net;            // non-empty: all pins belong to this net ("0" for ground)
    std::string ties_net_parameter;  // non-empty: all pins belong to the net named by this parameter (block ports)
    std::optional<Interactive> interactive;
    std::optional<Readout> readout;
    std::optional<McuBoard> mcu;  // unconnected pins of a board are not reported
    std::filesystem::path source; // file it was loaded from
    // A user block (PRJ-06): the circuit behind the symbol, whose port parts are
    // the pins. It has no template; the netlist puts the circuit in its place.
    std::shared_ptr<const model::Schematic> block;

    const Pin* pin(const std::string& pin_name) const;
    const Parameter* parameter(const std::string& parameter_name) const;
    // Parameter values of a new instance.
    std::map<std::string, std::string> default_parameters() const;
};

// Whether a model fits a model parameter's kinds (see Parameter::model_kinds).
bool model_fits(const Parameter& parameter, const SpiceModel& model);

struct LoadMessage {
    std::filesystem::path file;
    std::string text;
};

class ComponentLibrary {
public:
    // Loads every *.json below `directory` and the SPICE models of every *.lib
    // file there. Invalid files are reported and skipped; the rest of the
    // library stays usable.
    std::vector<LoadMessage> load_directory(const std::filesystem::path& directory);
    // Parses one definition; returns an error message on failure.
    std::optional<std::string> add_from_json(const std::string& text, const std::filesystem::path& source = {});
    // Adds a definition made in code (a block's), replacing one of the same type.
    void add_definition(ComponentDefinition definition);

    const ComponentDefinition* find(const std::string& type) const;
    const std::vector<ComponentDefinition>& definitions() const { return definitions_; }
    // Categories in display order (fixed order first, then alphabetical).
    std::vector<std::string> categories() const;
    bool empty() const { return definitions_.empty(); }

    // Built-in SPICE models (PRJ-07), in file order.
    const std::vector<SpiceModel>& models() const { return models_; }
    const SpiceModel* model(const std::string& type, const std::string& name) const;
    void add_models(const std::vector<SpiceModel>& models);

private:
    std::vector<ComponentDefinition> definitions_;
    std::vector<SpiceModel> models_;
};

// Schematic position of a pin of a placed component.
model::Point pin_position(const ComponentDefinition& definition, const Pin& pin, const model::Component& component);

// Library folder candidates for an application in `application_dir`.
std::vector<std::filesystem::path> library_candidates(const std::filesystem::path& application_dir);

} // namespace simnodus::library
