// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "engine_check.hpp"

#include "ngspice_runtime.hpp"

#include <cmath>
#include <memory>
#include <mutex>

namespace simnodus::sim {
namespace {

std::mutex check_mutex;
std::unique_ptr<NgspiceSession> engine;
std::filesystem::path engine_library;

} // namespace

EngineCheckResult check_analog_engine(const std::filesystem::path& application_dir)
{
    std::lock_guard lock(check_mutex);
    EngineCheckResult result;
    if (!engine) {
        for (const auto& candidate : ngspice_candidates(application_dir)) {
            std::string error;
            engine = NgspiceSession::open(candidate, error);
            if (engine) {
                engine_library = candidate.library;
                break;
            }
            result.messages.push_back(error);
        }
        if (!engine) {
            result.messages.push_back("No usable ngspice library was found.");
            return result;
        }
    }
    result.library = engine_library;
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
