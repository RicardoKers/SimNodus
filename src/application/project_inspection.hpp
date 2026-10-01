// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#pragma once
#include "application/project_graph.hpp"
#include <optional>
#include <span>

namespace simnodus {
// Read the loader-owned validated snapshot by source definition identity.
// Rows are immutable and borrowed: retain the graph for their entire lifetime.
// Paths include the root ID; origins describe the immediate declaration binding.
std::vector<const ParameterOccurrence*> inspect_instance_parameters(
    const ProjectGraph& graph, std::string_view circuit_id, std::string_view instance_id);

struct DeclaredLocalNetView {
    std::string id, name;
    std::vector<std::string> path;
};
struct DeclaredTerminalView {
    std::string pin, name;
    std::optional<DeclaredLocalNetView> net;
};
struct ComponentOccurrenceView {
    std::vector<std::string> path;
    std::string source_circuit, source_instance, component, name;
    std::map<std::string, EffectiveParameter> parameters;
    // Every declared logical pin, sorted by ID. A missing net means no local
    // membership in this source circuit, never a flattened electrical verdict.
    std::vector<DeclaredTerminalView> terminals;
};
// Pure full-ID-path lookup. Walk connectivity and cross-check the current
// resolved snapshot/source definition; return owned metadata, never a position.
std::optional<ComponentOccurrenceView> inspect_component_occurrence(
    const ProjectGraph& graph, std::span<const std::string> path);

enum class DeclaredEndpointKind { local_port, component_pin, circuit_port };
struct DeclaredLocalEndpointView {
    DeclaredEndpointKind kind;
    std::string instance, terminal, name, definition;
    std::vector<std::string> path;
};
struct DeclaredLocalNetDetailsView {
    DeclaredLocalNetView net;
    std::vector<DeclaredLocalEndpointView> endpoints;
};
// Inspect all direct members, including the selected pin, of its declared local
// net. Empty/unconnected selections return no details; ports are never traversed.
// Owned paths require their explicit endpoint kind; names do not establish identity.
std::optional<DeclaredLocalNetDetailsView> inspect_occurrence_local_net(
    const ProjectGraph& graph, std::span<const std::string> component_path, std::string_view pin);
}
