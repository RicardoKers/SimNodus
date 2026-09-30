// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#pragma once
#include "application/project_graph.hpp"
#include <optional>

namespace simnodus {
enum class ProjectRevisionStage { base, revision };
struct ProjectRevisionError {
    ProjectRevisionStage stage;
    const char* code;
    std::size_t offset;
};
using ProjectRevisionResult = std::variant<std::shared_ptr<const ProjectGraph>, ProjectRevisionError>;
// Rename only the top-level display name in an owned validated source revision.
// Borrowed inputs must stay stable during this call. No I/O or save authority.
// Base errors use original offsets; revision errors use candidate offsets.
ProjectRevisionResult rename_project(std::string_view original, std::string_view name_utf8);
// Rename one declared instance in a circuit definition, selected by stable IDs.
// This changes definition metadata in every occurrence, not an occurrence
// override. Only its immediate name value token changes; no I/O or authority.
ProjectRevisionResult rename_instance(std::string_view original,
    std::string_view circuit_id, std::string_view instance_id, std::string_view name_utf8);
struct LiteralResistanceView {
    std::string_view value, unit, minimum, maximum, base_unit;
};
// Read only a loader-owned validated graph. Strings borrow that graph's syntax;
// retain the graph or copy them before any edit, restoration or successful Open.
std::optional<LiteralResistanceView> literal_resistance(const ProjectGraph& graph,
    std::string_view circuit_id, std::string_view instance_id);
// Revise one existing literal resistance value; its declared unit stays fixed.
// Missing/default/forwarded bindings are not converted. Full existing decimal,
// dimension/range and project validation applies; no I/O or execution authority.
ProjectRevisionResult revise_resistance(std::string_view original,
    std::string_view circuit_id, std::string_view instance_id, std::string_view value);
}
