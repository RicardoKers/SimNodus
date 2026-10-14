// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#pragma once
#include "application/parameter_validation.hpp"

namespace simnodus {
struct BindingDeclaration {
    ParameterSnapshot parameters;
    std::size_t descriptor_entries, symbol_bindings, model_bindings;
    // Declaration validity never establishes executable simulation readiness.
    bool simulation_ready = false;
};
using BindingResult = std::variant<std::shared_ptr<const BindingDeclaration>, ParameterError>;
// Topology 0.3 only. Owns original catalogs/maps/nulls and their source positions
// through parameters.topology.syntax; no resource I/O or source-interface check.
BindingResult validate_binding_declaration(std::string_view bytes);
}
