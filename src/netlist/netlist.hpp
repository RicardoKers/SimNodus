// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// SPICE netlist generation and pre-run checks (SIM-05). The result keeps the
// mapping from SPICE names back to schematic IDs, so that diagnostics,
// probes and live values refer to components and nets, never to ngspice
// names.
#pragma once

#include "connectivity.hpp"
#include "library/component_library.hpp"
#include "model/schematic.hpp"

#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace simnodus::netlist {

enum class Severity { Info, Warning, Error };

struct Diagnostic {
    Severity severity = Severity::Error;
    std::string code;    // stable key, e.g. "no-ground"
    std::string message; // plain-language text for the Messages panel
    std::vector<Id> items;
    std::optional<Point> at;
};

// One SPICE element produced by a component template.
struct Element {
    Id component = model::no_id;
    std::string name;  // as written in the netlist, e.g. "C_C1"
    char letter = 'X'; // element kind, upper case
    std::vector<std::string> nodes;
    std::string value_text; // rest of the line after the nodes
};

// External voltage source driving an interactive control (ADR 0080).
struct ControlSource {
    Id component = model::no_id;
    std::string source;    // element name, e.g. "V_ctl_SW1"
    std::string node;      // control node
    double initial = 0.0;  // 0/1, or 0..1 for a potentiometer
    library::Interactive::Kind kind = library::Interactive::Kind::Toggle;
};

struct NetlistOptions {
    bool interactive = true; // control sources use the ngspice "external" option
    double t0 = 0.0;         // segment start time for periodic sources
    // Pins whose current is measured: a 0 V source is inserted between the
    // net and the pin; its branch current is the current into the pin.
    std::vector<PinRef> current_probes;
};

struct Netlist {
    std::vector<std::string> lines;  // element lines, without title, analysis or .end
    std::vector<std::string> models; // unique .model lines
    std::vector<Element> elements;
    std::vector<ControlSource> controls;
    Connectivity connectivity;
    std::map<PinRef, std::string> pin_node;
    // Current probes: pin -> ngspice vector of the current into the pin.
    std::map<PinRef, std::string> pin_current;
    std::vector<Diagnostic> diagnostics;

    bool ok() const;
    // Number of diagnostics at or above `severity`.
    std::size_t count(Severity severity) const;
};

Netlist generate_netlist(const model::Schematic& schematic, const library::ComponentLibrary& library,
    const NetlistOptions& options = {});

// Element and control lines plus models; `extra` lines (analysis, .save)
// go before .end. ngspice reads the first line as the title.
std::vector<std::string> build_deck(const Netlist& netlist, const std::string& title,
    const std::vector<std::string>& extra);

} // namespace simnodus::netlist
