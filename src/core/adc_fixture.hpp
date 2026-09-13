// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#pragma once
#include "backend_contracts.hpp"
namespace simnodus {
struct AdcFixtureInput {
    static constexpr Nanoseconds boundary_ns = 4'010'750;
    static constexpr std::uint32_t channel = 0;
    std::uint32_t microvolts = 0, expected_code = 0;
    static bool prepare(double volts, AdcFixtureInput& result) noexcept
    {
        if(!std::isfinite(volts) || volts < 0 || volts > 3.3) return false;
        const auto uv = static_cast<std::uint64_t>(std::floor(volts * 1'000'000 + 0.5));
        const auto code = uv * 4096 / 3'300'000;
        result = {static_cast<std::uint32_t>(uv), static_cast<std::uint32_t>(code > 4095 ? 4095 : code)};
        return true;
    }
};
} // namespace simnodus
