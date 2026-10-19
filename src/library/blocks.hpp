// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// Blocks drawn by the user (PRJ-06, ADR 0098). A block is an ordinary project
// whose "Block port" parts name its pins. Another project keeps a copy of it
// (model::Block) and shows it as a rectangle with the ports as pins: left and
// right ports from top to bottom, top and bottom ports from left to right, in
// the order they have on the block's schematic.
#pragma once

#include "component_library.hpp"
#include "model/schematic.hpp"

#include <optional>
#include <string>
#include <vector>

namespace simnodus::library {

// Component type of the port part; its "name" parameter names the pin and
// "side" places it on the symbol.
inline constexpr const char* port_type = "port";

struct BlockPort {
    std::string name;
    std::string side; // "left", "right", "top" or "bottom"
    model::Id component = model::no_id;
    model::Point at; // the port's position on the block's schematic
};

// Ports of a block's circuit, one per name, in symbol order. `problems` says
// why the circuit cannot be a block (no ports, a port named GND, a
// microcontroller board inside); the list is then empty.
std::vector<BlockPort> block_ports(const model::Schematic& circuit, const ComponentLibrary& library,
    std::vector<std::string>& problems);

// The block's symbol and pins as a component definition ("block:<name>").
std::optional<ComponentDefinition> block_definition(const model::Block& block, const ComponentLibrary& library,
    std::vector<std::string>& problems);

// `base` with the definitions of the schematic's blocks added. A block that
// cannot be used is left out and explained in `problems`.
ComponentLibrary with_blocks(const ComponentLibrary& base, const model::Schematic& schematic,
    std::vector<std::string>* problems = nullptr);

} // namespace simnodus::library
