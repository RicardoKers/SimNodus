// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#pragma once
#include "application/topology_validation.hpp"

namespace simnodus::detail {
// Internal structural phase only. Input must come directly from native ingress.
// Throws TopologyError; parameter semantics must succeed before publication.
TopologyDeclaration parameter_topology(std::shared_ptr<const DeclarationSyntax> source);
}
