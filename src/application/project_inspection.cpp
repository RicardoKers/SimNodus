// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "application/project_inspection.hpp"
#include <algorithm>

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
}
