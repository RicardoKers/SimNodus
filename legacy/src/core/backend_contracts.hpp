// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#pragma once
#include <cmath>
#include <cstdint>
namespace simnodus {
using Nanoseconds = std::uint64_t;
// An analog observation is neither a CPU acknowledgement nor a joint commit.
struct AnalogObservation {
    Nanoseconds requested_ns;
    double observed_seconds;
    double output_volts;
};
enum class BoundaryError { none, invalid_request, nonfinite, endpoint_mismatch };
struct RcFixtureContract {
    static constexpr Nanoseconds limit_ns = 10'000'000;
    static constexpr double endpoint_tolerance_s = 1e-12;
    static constexpr double voltage_tolerance_v = 1e-5;
    static constexpr bool rollback = false;
    static constexpr bool joint_pause = false;
    static constexpr bool general_feedback = false;
    static BoundaryError request(Nanoseconds previous, Nanoseconds target, Nanoseconds limit) noexcept
    {
        return limit > limit_ns || target <= previous || target >= limit
            ? BoundaryError::invalid_request : BoundaryError::none;
    }
    static BoundaryError observation(const AnalogObservation& value) noexcept
    {
        if(!std::isfinite(value.observed_seconds) || !std::isfinite(value.output_volts))
            return BoundaryError::nonfinite;
        return std::abs(value.observed_seconds - static_cast<double>(value.requested_ns) * 1e-9)
                <= endpoint_tolerance_s
            ? BoundaryError::none : BoundaryError::endpoint_mismatch;
    }
};
} // namespace simnodus
