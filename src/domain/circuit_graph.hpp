// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#pragma once
#include <string>
#include <variant>
#include <vector>

namespace simnodus {
// Source connectivity only. Names never establish electrical identity.
enum class PinDomain { electrical };
enum class PortDirection { input, output, bidirectional };
enum class InstanceKind { component, circuit };
struct GraphIdentity { std::string id, name; };
struct GraphPin { GraphIdentity identity; PinDomain domain; };
struct GraphPort { GraphIdentity identity; PinDomain domain; PortDirection direction; };
struct GraphInstance { GraphIdentity identity; InstanceKind kind; std::string definition; };
struct LocalPortTerminal { std::string port; };
struct InstanceTerminal { std::string instance, terminal; };
using GraphTerminal = std::variant<LocalPortTerminal, InstanceTerminal>;
struct GraphNet { GraphIdentity identity; std::vector<GraphTerminal> terminals; };
struct GraphComponent { GraphIdentity identity; std::vector<GraphPin> pins; };
struct GraphCircuit {
    GraphIdentity identity;
    std::vector<GraphPort> ports;
    std::vector<GraphInstance> instances;
    std::vector<GraphNet> nets;
};
struct CircuitGraph {
    std::string root;
    std::vector<GraphComponent> components;
    std::vector<GraphCircuit> circuits;
};
}
