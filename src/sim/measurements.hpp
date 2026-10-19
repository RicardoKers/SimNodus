// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// Signal measurements and trigger search for the Signal Analyzer (AN-03,
// AN-04). Samples are irregular in time (SPICE steps), so averages are
// time-weighted (trapezoidal rule).
#pragma once

#include "sample_ring.hpp"

#include <optional>
#include <utility>
#include <vector>

namespace simnodus::sim {

struct Measurements {
    double min = 0.0;
    double max = 0.0;
    double mean = 0.0;
    double rms = 0.0;
    double peak_to_peak = 0.0;
    std::optional<double> frequency; // from crossings of the mid level
    std::optional<double> duty;      // fraction of time above the mid level
    std::size_t count = 0;
};

// Measurements of the samples within [from, to]; nullopt with fewer than two samples.
std::optional<Measurements> measure(const std::vector<Sample>& samples, double from, double to);

// Linear interpolation; nullopt outside the sampled range.
std::optional<double> value_at(const std::vector<Sample>& samples, double time);

// Timing of a digital signal (logic analyzer lane) within [from, to], from its
// transitions (time, new level) and its level at `from`.
struct LogicMeasurements {
    int rising = 0;                   // rising edges in the window
    int falling = 0;
    std::optional<double> period;     // mean time between rising edges
    std::optional<double> frequency;  // 1 / period
    std::optional<double> high_time;  // mean length of complete high pulses
    std::optional<double> low_time;
    std::optional<double> duty;       // fraction of the window spent high
};
LogicMeasurements measure_logic(bool initial, const std::vector<std::pair<double, bool>>& changes, double from,
    double to);
// Level of a digital signal at `time`.
bool logic_level_at(bool initial, const std::vector<std::pair<double, bool>>& changes, double time);

// Time of the last crossing of `level` (rising or falling) within [from, to].
std::optional<double> find_trigger(const std::vector<Sample>& samples, double level, bool rising, double from, double to);

} // namespace simnodus::sim
