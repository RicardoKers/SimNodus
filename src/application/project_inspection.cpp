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
}
