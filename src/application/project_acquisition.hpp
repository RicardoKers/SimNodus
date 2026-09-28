// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#pragma once
#include "application/project_graph.hpp"
#include "application/local_resources.hpp"
#include <cstdint>

namespace simnodus {
enum class ProjectAcquisitionStage { physical, declaration };
struct ProjectAcquisitionError {
    ProjectAcquisitionStage stage;
    const char* code;
    std::size_t offset = 0;
    std::uint32_t system_code = 0;
};
using ProjectAcquisitionResult = std::variant<std::shared_ptr<const ProjectGraph>, ProjectAcquisitionError>;
struct AcquiredProject {
    std::shared_ptr<const ProjectGraph> graph;
    PhysicalRootIdentity root_identity;
};
using BoundProjectAcquisitionResult = std::variant<AcquiredProject, ProjectAcquisitionError>;
// Explicit bounded capture of one selected document, then complete validation.
// Borrowed root/name must remain stable during this call. No resource I/O,
// execution, pathname lease or overwrite authority is granted by the result.
ProjectAcquisitionResult acquire_project(const std::string& root_utf8, const std::string& filename);
// The observed identity comes from the selected root handle retained during
// document capture. It is an observation, not authorization to use the root.
BoundProjectAcquisitionResult acquire_project_with_root_identity(
    const std::string& root_utf8, const std::string& filename);
}
