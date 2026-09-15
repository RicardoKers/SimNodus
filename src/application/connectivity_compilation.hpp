// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#pragma once
#include "application/project_graph.hpp"
#include "domain/connectivity_partition.hpp"

namespace simnodus {
inline constexpr std::size_t connectivity_max_records = 65536;
struct ConnectivityCompilation {
    std::shared_ptr<const ProjectGraph> source;
    std::vector<CompiledOccurrence> occurrences;
    std::vector<ConnectivityGroup> connections;
    std::map<OccurrencePath, GraphSourceSpan> occurrence_sources;
    std::map<CompiledTerminal, GraphSourceSpan> terminal_sources;
    std::map<CompiledNet, GraphSourceSpan> net_sources;
};
enum class ConnectivityCompilationStage { declaration, expansion };
struct ConnectivityCompilationError {
    ConnectivityCompilationStage stage;
    const char* code;
    std::size_t offset;
};
using ConnectivityCompilationResult = std::variant<std::shared_ptr<const ConnectivityCompilation>, ConnectivityCompilationError>;
// Validate complete source, then expand/partition explicit connectivity only.
// No resource I/O, backend netlist, ground inference or execution approval.
ConnectivityCompilationResult compile_connectivity(std::string_view original);
}
