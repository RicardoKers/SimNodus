// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#pragma once
#include "backend_contracts.hpp"
namespace simnodus {
// Analytical oracle for the owned single-edge fixture, not a circuit solver.
struct RcTrajectory {
    static constexpr Nanoseconds rise_ns = 2'011'375;
    static bool accepts(const AnalogObservation& sample, bool high) noexcept
    {
        if(RcFixtureContract::observation(sample) != BoundaryError::none) return false;
        if(high ? sample.requested_ns < rise_ns : sample.requested_ns > rise_ns) return false;
        const double expected = high
            ? 3.3 * -std::expm1(-static_cast<double>(sample.requested_ns - rise_ns) * 1e-9 / 0.001)
            : 0.0;
        return std::abs(sample.output_volts - expected) <= RcFixtureContract::voltage_tolerance_v;
    }
};
} // namespace simnodus
