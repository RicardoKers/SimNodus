// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#pragma once
#include "application/topology_validation.hpp"
#include <map>

namespace simnodus {
struct EffectiveParameter {
    std::string value, unit, origin;
    std::size_t source_offset;
};
struct ParameterOccurrence {
    std::vector<std::string> path;
    std::string definition;
    std::map<std::string, EffectiveParameter> parameters;
    std::size_t source_offset;
};
struct ParameterSnapshot {
    TopologyDeclaration topology;
    std::size_t parameter_entries, resolved_entries;
    std::vector<ParameterOccurrence> instances;
};
struct ParameterError { const char* code; std::size_t offset; };
using ParameterResult = std::variant<std::shared_ptr<const ParameterSnapshot>, ParameterError>;
// Topology 0.2 parameter inspection only; caller bytes must remain stable during the call.
ParameterResult resolve_parameter_declaration(std::string_view bytes);
}
