// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#pragma once
#include "application/resource_links.hpp"

namespace simnodus {
struct ProjectDeclaration {
    ResourceLinkDeclaration sources;
    std::string project_id;
    std::size_t targets, project_entries;
    bool runtime_profile_verified = false;
    bool firmware_verified = false;
};
using ProjectResult = std::variant<std::shared_ptr<const ProjectDeclaration>, LockError>;
// Project 0.1 declarations only; retain the original shared syntax, not an
// editable graph or an execution capability. No filesystem or backend access.
ProjectResult validate_project_declaration(std::string_view bytes);
}
