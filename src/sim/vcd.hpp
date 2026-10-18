// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// Value Change Dump (IEEE 1364) of digital signals for analysis in tools such
// as PulseView or GTKWave (AN-10). Times are in nanoseconds, the finest step
// the logic lanes keep.
#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace simnodus::sim {

struct VcdSignal {
    std::string name;                                     // "U1.PA0", "V(OUT)"
    bool initial = false;                                 // level at the dump start
    std::vector<std::pair<std::uint64_t, bool>> changes; // nanoseconds, new level
};

// Reference name usable in a VCD: letters, digits and '_' only ("U1.PA0" ->
// "U1_PA0", "V(OUT)" -> "V_OUT").
std::string vcd_name(const std::string& name);

// The dump starts at `start_ns`; changes at or before it set the initial
// value, later ones are written in time order, only when the level changes.
// Names are made unique with a numeric suffix.
std::string write_vcd(const std::vector<VcdSignal>& lanes, std::uint64_t start_ns, const std::string& version,
    const std::string& comment);

} // namespace simnodus::sim
