// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#pragma once
#include "application/declaration_ingress.hpp"

namespace simnodus {
struct TopologyDeclaration {
    std::shared_ptr<const DeclarationSyntax> syntax;
    std::string root;
    std::size_t declared_entities, root_depth, root_expanded_instances;
};
enum class TopologyErrorCode { input, shape, version, id, value, reference, connection, hierarchy, budget, memory };
struct TopologyError { TopologyErrorCode code; std::size_t offset; };
using TopologyResult = std::variant<TopologyDeclaration, TopologyError>;
// Complete topology 0.1 semantics only; no project/resource/runtime approval.
TopologyResult validate_topology_declaration(std::string_view bytes);
const char* topology_error_name(TopologyErrorCode code);
}
