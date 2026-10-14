// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// Explicit, user-requested check that the analog engine can be loaded and
// solves a reference circuit. It is the only engine entry point of the
// phase 0 skeleton; it is never called while a project is opened.
#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace simnodus::sim {

struct EngineCheckResult {
    bool ok = false;
    std::filesystem::path library;
    std::optional<int> version;
    double divider_volts = 0.0; // V(out) of a 3.3 V, 1k/1k divider; expected 1.65
    std::vector<std::string> messages;
};

// Loads ngspice from the first candidate that works (ngspice_candidates) and
// solves a resistive divider. Blocking; run it outside the GUI thread. The
// engine stays loaded afterwards, so repeated checks reuse the first result's
// library and only repeat the circuit solve.
EngineCheckResult check_analog_engine(const std::filesystem::path& application_dir);

} // namespace simnodus::sim
