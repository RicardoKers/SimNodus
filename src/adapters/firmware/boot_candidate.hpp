// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#pragma once
#include "adapters/firmware/elf_inspection.hpp"

namespace simnodus::firmware {
enum class BootProfile { unspecified, sn012_stm32f103c8 };
struct BootCandidate {
    std::shared_ptr<const Elf32Inspection> elf;
    BootProfile profile;
    std::string declared_architecture;
    std::uint32_t stack, reset_vector;
    std::size_t vector_file_offset;
};
struct BootCandidateError { const char* stage; const char* code; std::size_t offset; };
using BootCandidateResult = std::variant<std::shared_ptr<const BootCandidate>, BootCandidateError>;
// Pure static candidate inspection. Explicit reference profile, no firmware load,
// memory writes, instruction/section validation, device/runtime or execution grant.
// Use captured bytes; no pathname is accepted. Borrowed inputs stay stable in call.
BootCandidateResult inspect_boot_candidate(std::string_view image,
    std::string_view declared_architecture, BootProfile profile);
}
