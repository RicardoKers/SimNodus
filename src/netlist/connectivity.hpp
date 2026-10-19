// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// Nets of a schematic, derived from geometry (UI-06, UI-07):
// - a pin connects to every pin, wire and label at its point, including a
//   wire passing through the point;
// - a wire connects to another wire where a vertex or end of one lies on the
//   other (T connection); a plain crossing does not connect;
// - net labels with the same name connect; a ground symbol, or a label named
//   0 or GND, is the reference net "0";
// - a splitter's bit pin is a label named after its bus and bit ("D3", UI-19);
//   buses themselves are drawings and connect nothing;
// - a block port is a label named by its "name" parameter (ties_net_parameter).
#pragma once

#include "library/component_library.hpp"
#include "model/schematic.hpp"

#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace simnodus::netlist {

using model::Id;
using model::Point;

using PinRef = std::pair<Id, std::string>; // component ID, pin name

// Labels whose name starts with this mark connect like any label but never
// name a net. The netlist adds them where it puts a block's circuit (ADR 0098),
// so that the nets of the schematic keep the names the editor shows.
inline constexpr char hidden_label_mark = '\x1f';

struct Net {
    std::string name;       // SPICE node: "0", a sanitized label name, or N001...
    bool ground = false;
    std::vector<PinRef> pins;
    std::vector<Id> wires;
    std::vector<Id> labels;
    std::vector<std::string> label_names; // all label names on this net
    int taps = 0;                         // splitter bit pins on this net
};

struct Connectivity {
    std::vector<Net> nets;
    std::map<PinRef, int> pin_net;
    std::map<Id, int> wire_net;
    std::map<Id, int> label_net;
    std::map<Point, int> tap_net; // splitter bit pins
    std::vector<Point> junctions;      // three or more connections meet: draw a dot
    std::vector<Point> dangling_ends;  // wire ends connected to nothing
    std::vector<PinRef> unconnected_pins;

    // Net at a schematic point (pin, wire or label there), if any.
    std::optional<int> net_at(const model::Schematic& schematic, const library::ComponentLibrary& library,
        Point point) const;
    const Net* net_of_pin(const PinRef& pin) const;
};

Connectivity build_connectivity(const model::Schematic& schematic, const library::ComponentLibrary& library);

// SPICE-safe identifier: letters, digits and '_' (others become '_').
std::string sanitize_name(const std::string& text);

} // namespace simnodus::netlist
