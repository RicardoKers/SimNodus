// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "engine_check.hpp"

#include "engine_host.hpp"

#include <cmath>

namespace simnodus::sim {

EngineCheckResult check_analog_engine(const std::filesystem::path& application_dir)
{
    EngineCheckResult result;
    auto& host = EngineHost::instance();
    host.set_application_dir(application_dir);
    std::string error;
    std::lock_guard lock(host.lock());
    NgspiceSession* engine = host.acquire(error);
    if (!engine) {
        result.messages.push_back(error);
        result.messages.push_back("No usable ngspice library was found.");
        return result;
    }
    result.library = host.library_path();
    result.version = engine->version();
    const bool loaded = engine->load_circuit({
        "* SimNodus engine check",
        "V1 in 0 DC 3.3",
        "R1 in out 1k",
        "R2 out 0 1k",
        ".op",
        ".end",
    });
    if (!loaded || !engine->command("run")) {
        for (auto& line : engine->take_output()) {
            result.messages.push_back(std::move(line));
        }
        result.messages.push_back("The engine rejected the reference circuit.");
        engine->command("remcirc");
        return result;
    }
    const auto values = engine->vector("out");
    engine->command("destroy all");
    engine->command("remcirc");
    engine->take_output();
    if (values.empty()) {
        result.messages.push_back("The engine returned no operating point.");
        return result;
    }
    result.divider_volts = values.front();
    result.ok = std::abs(result.divider_volts - 1.65) < 1e-6;
    if (!result.ok) {
        result.messages.push_back("Unexpected divider voltage.");
    }
    return result;
}

} // namespace simnodus::sim
