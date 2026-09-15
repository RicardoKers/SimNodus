// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#pragma once
#include "application/binding_validation.hpp"
#include "application/lock_validation.hpp"

namespace simnodus::detail {
// Internal composition only: source/index must be an untouched native ingress
// capture and an existing subtree root. Errors throw; no partial publication.
BindingDeclaration captured_bindings(std::shared_ptr<const DeclarationSyntax> source, std::size_t root);
LockDeclaration captured_lock(std::shared_ptr<const DeclarationSyntax> source, std::size_t root);
}
