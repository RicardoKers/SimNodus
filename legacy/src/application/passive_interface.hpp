// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#pragma once
#include "application/resource_links.hpp"
#include "adapters/ngspice/passive_source.hpp"

namespace simnodus {
struct PassiveInterface {
    std::shared_ptr<const ResourceLinkDeclaration> declaration;
    std::shared_ptr<const spice::PassiveSource> source;
    std::size_t model_index, descriptor_offset, binding_offset;
    std::string descriptor, asset, dependency, resource;
    // Descriptor IDs in actual source formal-terminal order, never array order.
    std::array<std::string, 2> terminal_ids;
    std::string parameter_id, unit, minimum, maximum;
};
enum class PassiveInterfaceStage { declaration, selection, source, correspondence };
struct PassiveInterfaceError { PassiveInterfaceStage stage; const char* code; std::size_t offset; };
using PassiveInterfaceResult = std::variant<std::shared_ptr<const PassiveInterface>, PassiveInterfaceError>;
// Own/revalidate metadata and selected bytes. No I/O, physical containment proof,
// effective/default value conversion, complete interface flag or execution grant.
PassiveInterfaceResult match_passive_interface(std::string_view resource_links,
    std::string_view descriptor_id, std::string_view captured_source);
}
