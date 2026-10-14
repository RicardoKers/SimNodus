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

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace simnodus::library {

enum class PinDirection { Left, Right, Up, Down };

struct Pin {
    std::string name;
    model::Point at; // grid units, relative to the symbol origin
    PinDirection direction = PinDirection::Up;
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
    std::string name;
    std::string label;
    std::string unit; // "Ohm", "F", "H", "V", "A", "Hz", "s", "%", "deg" or empty
    std::string default_value;
    std::optional<double> min;
    std::optional<double> max;
    std::vector<std::string> choices; // non-empty: the value is one of these words
    std::string description;
};

// Interactive control (INT-02). The netlist generator creates an external
// voltage source on node "{control}" whose value is the control state:
// 0/1 for toggle and button, 0..1 for a potentiometer position.
struct Interactive {
    enum class Kind { Toggle, Button, Potentiometer };
    Kind kind = Kind::Toggle;
    std::string parameter; // parameter holding the initial state ("closed", "position")
};

// Value shown on the symbol while running (INT-03).
struct Readout {
    enum class Kind { Voltage, Current };
    Kind kind = Kind::Voltage;
    std::vector<std::string> pins; // voltage: + and - pin
    std::string element;           // current: template element name, e.g. "V_{ref}"
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
    std::optional<Interactive> interactive;
    std::optional<Readout> readout;
    std::filesystem::path source; // file it was loaded from

    const Pin* pin(const std::string& pin_name) const;
    const Parameter* parameter(const std::string& parameter_name) const;
    // Parameter values of a new instance.
    std::map<std::string, std::string> default_parameters() const;
};

struct LoadMessage {
    std::filesystem::path file;
    std::string text;
};

class ComponentLibrary {
public:
    // Loads every *.json below `directory`. Invalid files are reported and
    // skipped; the rest of the library stays usable.
    std::vector<LoadMessage> load_directory(const std::filesystem::path& directory);
    // Parses one definition; returns an error message on failure.
    std::optional<std::string> add_from_json(const std::string& text, const std::filesystem::path& source = {});

    const ComponentDefinition* find(const std::string& type) const;
    const std::vector<ComponentDefinition>& definitions() const { return definitions_; }
    // Categories in display order (fixed order first, then alphabetical).
    std::vector<std::string> categories() const;
    bool empty() const { return definitions_.empty(); }

private:
    std::vector<ComponentDefinition> definitions_;
};

// Schematic position of a pin of a placed component.
model::Point pin_position(const ComponentDefinition& definition, const Pin& pin, const model::Component& component);

// Library folder candidates for an application in `application_dir`.
std::vector<std::filesystem::path> library_candidates(const std::filesystem::path& application_dir);

} // namespace simnodus::library
