// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "application/project_inspection.hpp"
#include <algorithm>
#include <tuple>

namespace simnodus {
std::vector<const ParameterOccurrence*> inspect_instance_parameters(
    const ProjectGraph& graph, std::string_view circuit_id, std::string_view instance_id)
{
    std::vector<const ParameterOccurrence*> rows;
    if(circuit_id.empty() || instance_id.empty() || circuit_id.size() > 64 || instance_id.size() > 64)
        return rows;
    const auto source = graph.source_map.find({"circuits", std::string(circuit_id), "instances", std::string(instance_id)});
    if(source == graph.source_map.end()) return rows;
    for(const auto& row : graph.declaration->sources.topology.parameters.instances)
        if(row.source_offset == source->second.begin) rows.push_back(&row);
    std::sort(rows.begin(), rows.end(), [](const auto* left, const auto* right) { return left->path < right->path; });
    return rows;
}
std::optional<ComponentOccurrenceView> inspect_component_occurrence(
    const ProjectGraph& graph, std::span<const std::string> path)
{
    if(path.size() < 2 || path.front() != graph.connectivity.root) return std::nullopt;
    std::string circuit_id = graph.connectivity.root;
    for(std::size_t index = 1; index < path.size(); ++index) {
        const auto circuit = std::find_if(graph.connectivity.circuits.begin(), graph.connectivity.circuits.end(),
            [&circuit_id](const auto& row) { return row.identity.id == circuit_id; });
        if(circuit == graph.connectivity.circuits.end()) return std::nullopt;
        const auto instance = std::find_if(circuit->instances.begin(), circuit->instances.end(),
            [&path, index](const auto& row) { return row.identity.id == path[index]; });
        if(instance == circuit->instances.end()) return std::nullopt;
        if(index + 1 < path.size()) {
            if(instance->kind != InstanceKind::circuit) return std::nullopt;
            circuit_id = instance->definition;
            continue;
        }
        if(instance->kind != InstanceKind::component) return std::nullopt;
        const auto component = std::find_if(graph.connectivity.components.begin(), graph.connectivity.components.end(),
            [&instance](const auto& row) { return row.identity.id == instance->definition; });
        if(component == graph.connectivity.components.end()) return std::nullopt;
        const auto source = graph.source_map.find({"circuits", circuit_id, "instances", instance->identity.id});
        if(source == graph.source_map.end()) return std::nullopt;
        for(const auto& row : graph.declaration->sources.topology.parameters.instances) {
            if(row.path.size() != path.size() || !std::equal(row.path.begin(), row.path.end(), path.begin())) continue;
            if(row.source_offset != source->second.begin || row.definition != instance->definition) return std::nullopt;
            ComponentOccurrenceView view{row.path, circuit_id, instance->identity.id,
                instance->definition, instance->identity.name, row.parameters, {}};
            for(const auto& pin : component->pins) view.terminals.push_back({pin.identity.id, pin.identity.name, {}});
            std::sort(view.terminals.begin(), view.terminals.end(), [](const auto& left, const auto& right) { return left.pin < right.pin; });
            // Only the directly containing source circuit is inspected. Ports,
            // parent nets, symbol maps and model terminals do not establish this membership.
            for(const auto& net : circuit->nets) for(const auto& endpoint : net.terminals) {
                const auto* terminal = std::get_if<InstanceTerminal>(&endpoint);
                if(!terminal || terminal->instance != instance->identity.id) continue;
                const auto selected = std::lower_bound(view.terminals.begin(), view.terminals.end(), terminal->terminal,
                    [](const auto& value, const auto& id) { return value.pin < id; });
                if(selected == view.terminals.end() || selected->pin != terminal->terminal || selected->net) return std::nullopt;
                std::vector<std::string> net_path(path.begin(), path.end() - 1);
                net_path.push_back(net.identity.id);
                selected->net = DeclaredLocalNetView{net.identity.id, net.identity.name, std::move(net_path)};
            }
            return view;
        }
    }
    return std::nullopt;
}
std::optional<DeclaredLocalNetDetailsView> inspect_occurrence_local_net(
    const ProjectGraph& graph, std::span<const std::string> component_path, std::string_view pin)
{
    if(pin.empty() || pin.size() > 64) return std::nullopt;
    const auto occurrence = inspect_component_occurrence(graph, component_path);
    if(!occurrence) return std::nullopt;
    const auto selected = std::find_if(occurrence->terminals.begin(), occurrence->terminals.end(),
        [pin](const auto& row) { return row.pin == pin; });
    if(selected == occurrence->terminals.end() || !selected->net) return std::nullopt;
    const auto by_id = [](const auto& rows, std::string_view id) {
        return std::find_if(rows.begin(), rows.end(), [id](const auto& row) { return row.identity.id == id; });
    };
    const auto circuit = by_id(graph.connectivity.circuits, occurrence->source_circuit);
    if(circuit == graph.connectivity.circuits.end()) return std::nullopt;
    const auto net = by_id(circuit->nets, selected->net->id);
    if(net == circuit->nets.end()) return std::nullopt;
    DeclaredLocalNetDetailsView result{*selected->net, {}};
    for(const auto& terminal : net->terminals) {
        DeclaredLocalEndpointView endpoint{};
        endpoint.path.assign(component_path.begin(), component_path.end() - 1);
        if(const auto* local = std::get_if<LocalPortTerminal>(&terminal)) {
            const auto port = by_id(circuit->ports, local->port);
            if(port == circuit->ports.end()) return std::nullopt;
            endpoint.kind = DeclaredEndpointKind::local_port;
            endpoint.terminal = port->identity.id; endpoint.name = port->identity.name;
            endpoint.definition = circuit->identity.id;
        } else {
            const auto* member = std::get_if<InstanceTerminal>(&terminal);
            if(!member) return std::nullopt;
            const auto instance = by_id(circuit->instances, member->instance);
            if(instance == circuit->instances.end()) return std::nullopt;
            endpoint.instance = instance->identity.id; endpoint.terminal = member->terminal;
            endpoint.definition = instance->definition;
            endpoint.path.push_back(endpoint.instance);
            if(instance->kind == InstanceKind::component) {
                const auto component = by_id(graph.connectivity.components, instance->definition);
                if(component == graph.connectivity.components.end()) return std::nullopt;
                const auto logical = by_id(component->pins, member->terminal);
                if(logical == component->pins.end()) return std::nullopt;
                endpoint.kind = DeclaredEndpointKind::component_pin; endpoint.name = logical->identity.name;
            } else {
                const auto definition = by_id(graph.connectivity.circuits, instance->definition);
                if(definition == graph.connectivity.circuits.end()) return std::nullopt;
                const auto port = by_id(definition->ports, member->terminal);
                if(port == definition->ports.end()) return std::nullopt;
                endpoint.kind = DeclaredEndpointKind::circuit_port; endpoint.name = port->identity.name;
            }
        }
        endpoint.path.push_back(endpoint.terminal);
        result.endpoints.push_back(std::move(endpoint));
    }
    std::sort(result.endpoints.begin(), result.endpoints.end(), [](const auto& left, const auto& right) {
        return std::tie(left.kind, left.instance, left.terminal) < std::tie(right.kind, right.instance, right.terminal);
    });
    return result;
}
}
