// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// Explicit, user-requested check that the microcontroller engine works:
// starts Renode with the STM32F103C8 platform (no firmware), advances 1 ms of
// virtual time and stops it. Never called while a project is opened.
#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace simnodus::mcu {

struct McuCheckResult {
    bool ok = false;
    std::filesystem::path renode;
    double startup_seconds = 0.0;
    std::vector<std::string> messages;
};

// Blocking (several seconds); run it outside the GUI thread.
McuCheckResult check_mcu_engine(const std::filesystem::path& application_dir);

} // namespace simnodus::mcu
