// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#pragma once
#include "application/project_validation.hpp"
#include "domain/circuit_graph.hpp"
#include <map>

namespace simnodus {
struct GraphSourceSpan { std::size_t begin, end; };
// Alternating collection/ID selectors, then explicit endpoint selectors for
// terminals. Independent of labels, array positions and JSON object key order.
using GraphSourceIdentity = std::vector<std::string>;
struct ProjectGraph {
    CircuitGraph connectivity;
    std::map<GraphSourceIdentity, GraphSourceSpan> source_map;
    std::shared_ptr<const ProjectDeclaration> declaration;
};
using ProjectGraphResult = std::variant<std::shared_ptr<const ProjectGraph>, LockError>;
// Validate complete project 0.1 before publishing an immutable source graph.
// Retain every metadata field in declaration; no paths/resources are opened.
ProjectGraphResult load_project_graph(std::string_view bytes);
}
