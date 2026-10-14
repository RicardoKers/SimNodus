// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#pragma once
#include "application/binding_validation.hpp"
#include "application/lock_validation.hpp"

namespace simnodus {
struct ResourceLinkDeclaration {
    BindingDeclaration topology;
    LockDeclaration lock;
    std::size_t descriptors_linked, assets;
    bool resource_interfaces_verified = false;
};
using ResourceLinkResult = std::variant<std::shared_ptr<const ResourceLinkDeclaration>, LockError>;
// Resource-links 0.1 declarations only. Both nested results retain the same
// original syntax, including asset roles, entrypoints, maps and explicit nulls.
ResourceLinkResult validate_resource_links(std::string_view bytes);
}
