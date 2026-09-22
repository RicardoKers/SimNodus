// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#pragma once
#include "application/project_validation.hpp"
#include "adapters/firmware/elf_inspection.hpp"

namespace simnodus {
struct FirmwareInspection {
    std::shared_ptr<const ProjectDeclaration> declaration;
    ResourceSnapshots resources;
    std::shared_ptr<const firmware::Elf32Inspection> elf;
    std::string firmware_id, declared_architecture;
    std::size_t firmware_offset, architecture_offset, resource_index;
};
enum class FirmwareInspectionStage { declaration, selection, resources, elf };
struct FirmwareInspectionError {
    FirmwareInspectionStage stage;
    const char* code;
    std::size_t offset = 0;
    std::uint32_t system = 0;
    const char* coordinate = "project";
};
using FirmwareInspectionResult = std::variant<std::shared_ptr<const FirmwareInspection>, FirmwareInspectionError>;
// Explicit complete project validation + physical capture + selected inert ELF
// inspection. Never reopen a verified pathname. No architecture equivalence,
// boot/device/runtime approval, engine call or firmware loading. Borrowed inputs
// must remain stable during this call. Declaration readiness flags stay false.
FirmwareInspectionResult inspect_project_firmware(std::string_view project,
    const std::string& root_utf8, std::string_view firmware_id);
}
