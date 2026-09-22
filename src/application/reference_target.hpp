// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#pragma once
#include "application/firmware_inspection.hpp"
#include "adapters/firmware/boot_candidate.hpp"
#include <map>

namespace simnodus {
struct ReferenceTarget {
    std::shared_ptr<const FirmwareInspection> firmware;
    std::shared_ptr<const firmware::BootCandidate> boot;
    std::vector<std::string> path;
    std::string platform_id;
    std::map<std::string, std::string> pin_map;
    std::size_t target_offset, platform_offset, platform_resource_index;
};
struct ReferenceTargetError {
    const char* stage;
    const char* code;
    std::size_t offset = 0;
    std::uint32_t system = 0;
    const char* coordinate = "project";
};
using ReferenceTargetResult = std::variant<std::shared_ptr<const ReferenceTarget>, ReferenceTargetError>;
// Explicit isolated SN-012 reference selection. Returns owned inert inputs only;
// no loader, process, electrical compilation or execution authority. Borrowed
// arguments must remain stable in call. All declaration readiness stays false.
ReferenceTargetResult inspect_reference_target(std::string_view project,
    const std::string& root, const std::vector<std::string>& path, firmware::BootProfile profile);
}
