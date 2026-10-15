// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "mcu_check.hpp"

#include "renode_engine.hpp"

#include <chrono>

namespace simnodus::mcu {

McuCheckResult check_mcu_engine(const std::filesystem::path& application_dir)
{
    McuCheckResult result;
    const auto renode = find_renode(application_dir);
    const auto resources = find_mcu_resources(application_dir);
    if (!renode) {
        result.messages.push_back("Renode was not found next to the application.");
        return result;
    }
    if (!resources) {
        result.messages.push_back("The microcontroller support files (mcu/) were not found.");
        return result;
    }
    result.renode = *renode;
    MachineOptions options;
    options.renode = *renode;
    options.resources = *resources;
    std::string error;
    const auto begin = std::chrono::steady_clock::now();
    auto engine = RenodeEngine::start(options, error);
    result.startup_seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - begin).count();
    if (!engine) {
        result.messages.push_back(error);
        return result;
    }
    const bool ran = engine->run_for(1000);
    const auto now = engine->time_us();
    if (!ran || now != std::uint64_t{1000}) {
        result.messages.push_back("Renode did not advance 1 ms of virtual time: " + engine->error());
        engine->stop();
        return result;
    }
    const auto code = engine->stop();
    if (code != 0) {
        result.messages.push_back("Renode did not exit cleanly.");
        return result;
    }
    result.ok = true;
    return result;
}

} // namespace simnodus::mcu
