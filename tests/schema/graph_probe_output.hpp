// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#pragma once
#include "application/project_graph.hpp"
#include <iostream>

namespace graph_probe {
inline void quote(const std::string& value)
{
    std::cout << '"';
    for(const auto c : value) {
        if(c == '"' || c == '\\') std::cout << '\\';
        std::cout << c;
    }
    std::cout << '"';
}
inline void identity(const simnodus::GraphIdentity& value)
{
    std::cout << "\"id\":"; quote(value.id);
    std::cout << ",\"name\":"; quote(value.name);
}
template<class Values, class Writer> void array(const Values& values, Writer writer)
{
    std::cout << '[';
    bool first = true;
    for(const auto& value : values) {
        if(!first) std::cout << ',';
        first = false;
        writer(value);
    }
    std::cout << ']';
}
inline void write(const simnodus::ProjectGraph& loaded)
{
    using namespace simnodus;
    const auto& graph = loaded.connectivity;
    std::cout << "\"graph\":{\"root\":"; quote(graph.root);
    std::cout << ",\"components\":";
    array(graph.components, [](const GraphComponent& component) {
        std::cout << '{'; identity(component.identity);
        std::cout << ",\"pins\":";
        array(component.pins, [](const GraphPin& pin) {
            std::cout << '{'; identity(pin.identity);
            std::cout << ",\"domain\":\"" << (pin.domain == PinDomain::electrical ? "electrical" : "invalid") << "\"}";
        });
        std::cout << '}';
    });
    std::cout << ",\"circuits\":";
    array(graph.circuits, [](const GraphCircuit& circuit) {
        std::cout << '{'; identity(circuit.identity);
        std::cout << ",\"ports\":";
        array(circuit.ports, [](const GraphPort& port) {
            std::cout << '{'; identity(port.identity);
            std::cout << ",\"domain\":\"" << (port.domain == PinDomain::electrical ? "electrical" : "invalid") << "\",\"direction\":\""
                << (port.direction == PortDirection::input ? "input" : port.direction == PortDirection::output ? "output" : "bidirectional") << "\"}";
        });
        std::cout << ",\"instances\":";
        array(circuit.instances, [](const GraphInstance& instance) {
            std::cout << '{'; identity(instance.identity);
            std::cout << ",\"kind\":\"" << (instance.kind == InstanceKind::component ? "component" : "circuit") << "\",\"definition\":";
            quote(instance.definition); std::cout << '}';
        });
        std::cout << ",\"nets\":";
        array(circuit.nets, [](const GraphNet& net) {
            std::cout << '{'; identity(net.identity);
            std::cout << ",\"terminals\":";
            array(net.terminals, [](const GraphTerminal& terminal) {
                if(const auto* local = std::get_if<LocalPortTerminal>(&terminal)) {
                    std::cout << "{\"port\":"; quote(local->port);
                } else {
                    const auto& pin = std::get<InstanceTerminal>(terminal);
                    std::cout << "{\"instance\":"; quote(pin.instance);
                    std::cout << ",\"terminal\":"; quote(pin.terminal);
                }
                std::cout << '}';
            });
            std::cout << '}';
        });
        std::cout << '}';
    });
    std::cout << "},\"positions\":";
    array(loaded.source_map, [](const auto& row) {
        std::cout << "{\"identity\":"; array(row.first, quote);
        std::cout << ",\"begin\":" << row.second.begin << ",\"end\":" << row.second.end << '}';
    });
    std::cout << ',';
}
}
