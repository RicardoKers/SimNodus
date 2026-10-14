// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#pragma once
#include "application/passive_interface.hpp"

namespace simnodus {
struct PassiveNumericOccurrence {
    std::vector<std::string> path;
    std::string definition, parameter;
    std::array<std::string, 2> pins;
    EffectiveParameter effective;
    std::size_t instance_offset;
};
struct PassiveNumericBinding {
    std::shared_ptr<const PassiveInterface> interface;
    std::string default_value;
    std::vector<PassiveNumericOccurrence> occurrences;
};
enum class PassiveNumericStage { interface, default_value, occurrence };
struct PassiveNumericError {
    PassiveNumericStage stage;
    const char* code;
    std::size_t offset;
    PassiveInterfaceStage interface_stage = PassiveInterfaceStage::declaration;
};
using PassiveNumericResult = std::variant<std::shared_ptr<const PassiveNumericBinding>, PassiveNumericError>;
// Revalidate inputs; exact positive R/C values only. No binary floating-point,
// netlist emission, physical containment claim or execution/readiness grant.
PassiveNumericResult bind_passive_numeric(std::string_view resource_links,
    std::string_view descriptor_id, std::string_view captured_source);
}
