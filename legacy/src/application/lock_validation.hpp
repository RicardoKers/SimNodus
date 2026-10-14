// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#pragma once
#include "application/declaration_ingress.hpp"
#include "application/local_resources.hpp"

namespace simnodus {
struct LockDeclaration {
    std::shared_ptr<const DeclarationSyntax> syntax;
    std::size_t dependencies;
    std::uint64_t declared_bytes;
    std::vector<ResourceRequest> requests;
    std::vector<std::size_t> source_offsets;
    bool resources_verified = false, containment_verified = false;
    bool redistribution_verified = false, simulation_ready = false;
};
struct LockError { const char* code; std::size_t offset; };
using LockResult = std::variant<std::shared_ptr<const LockDeclaration>, LockError>;
// Resource lock 0.1 metadata only. No root/path is opened; input must stay stable
// during this call. The immutable requests are declarations, never verified bytes.
LockResult validate_lock_declaration(std::string_view bytes);
}
