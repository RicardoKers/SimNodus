// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#pragma once
#include "application/project_graph.hpp"

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
}
