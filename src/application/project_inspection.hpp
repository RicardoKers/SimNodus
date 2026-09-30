// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#pragma once
#include "application/project_graph.hpp"

namespace simnodus {
// Read the loader-owned validated snapshot by source definition identity.
// Rows are immutable and borrowed: retain the graph for their entire lifetime.
// Paths include the root ID; origins describe the immediate declaration binding.
std::vector<const ParameterOccurrence*> inspect_instance_parameters(
    const ProjectGraph& graph, std::string_view circuit_id, std::string_view instance_id);
}
