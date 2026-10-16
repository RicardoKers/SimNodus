// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// A peripheral with events at known CPU cycles (timer updates, ADC conversion
// ends). The machine stops the core at the earliest next_event() and runs the
// devices' events in time order, so their effects come at the exact cycle.
#pragma once

#include <cstdint>
#include <limits>

namespace simnodus::emu {

inline constexpr std::uint64_t no_event = std::numeric_limits<std::uint64_t>::max();

class TimedDevice {
public:
    virtual ~TimedDevice() = default;
    // Cycle of the next event, or no_event.
    virtual std::uint64_t next_event() const = 0;
    // Processes the events due at or before `cycle`.
    virtual void on_time(std::uint64_t cycle) = 0;
};

} // namespace simnodus::emu
