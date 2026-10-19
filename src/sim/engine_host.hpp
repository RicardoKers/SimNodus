// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// The process-wide analog engine. ngspice has global state, so the
// application loads it once (on first use, never when a project is opened)
// and every user - the engine check and the simulation session - shares it.
#pragma once

#include "ngspice_runtime.hpp"

#include <filesystem>
#include <mutex>
#include <string>

namespace simnodus::sim {

class EngineHost {
public:
    static EngineHost& instance();

    // Where to look for the engine; call before the first acquire().
    void set_application_dir(const std::filesystem::path& application_dir);
    void set_location(const NgspiceLocation& location); // explicit location (tests)

    // Loads the engine on the first call, and again after ngspice asked to be
    // detached (NgspiceSession::needs_reload). Returns nullptr and a message if
    // no candidate works. The caller must hold lock() while using it.
    NgspiceSession* acquire(std::string& error);
    std::filesystem::path library_path() const;
    std::mutex& lock() { return use_; }

private:
    EngineHost() = default;
    mutable std::mutex state_;
    std::mutex use_;
    std::filesystem::path application_dir_;
    std::optional<NgspiceLocation> explicit_;
    std::unique_ptr<NgspiceSession> engine_;
    std::filesystem::path loaded_from_;
    std::string failure_;
};

} // namespace simnodus::sim
