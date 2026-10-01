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

struct ComponentOccurrenceView {
    std::vector<std::string> path;
    std::string source_circuit, source_instance, component, name;
    std::map<std::string, EffectiveParameter> parameters;
};
// Pure full-ID-path lookup. Walk connectivity and cross-check the current
// resolved snapshot/source definition; return owned metadata, never a position.
std::optional<ComponentOccurrenceView> inspect_component_occurrence(
    const ProjectGraph& graph, std::span<const std::string> path);
}
