// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "engine_host.hpp"

namespace simnodus::sim {

EngineHost& EngineHost::instance()
{
    static EngineHost host;
    return host;
}

void EngineHost::set_application_dir(const std::filesystem::path& application_dir)
{
    std::lock_guard lock(state_);
    application_dir_ = application_dir;
}

std::filesystem::path EngineHost::application_dir() const
{
    std::lock_guard lock(state_);
    return application_dir_;
}

void EngineHost::set_location(const NgspiceLocation& location)
{
    std::lock_guard lock(state_);
    explicit_ = location;
}

std::optional<NgspiceLocation> EngineHost::location() const
{
    std::lock_guard lock(state_);
    return explicit_;
}

NgspiceSession* EngineHost::acquire(std::string& error)
{
    std::lock_guard lock(state_);
    if (engine_ && engine_->needs_reload()) {
        engine_.reset(); // unloads the library; loaded again below
    }
    if (engine_) {
        return engine_.get();
    }
    std::vector<NgspiceLocation> candidates;
    if (explicit_) {
        candidates.push_back(*explicit_);
    } else {
        candidates = ngspice_candidates(application_dir_);
    }
    std::string messages;
    for (const auto& candidate : candidates) {
        std::string reason;
        engine_ = NgspiceSession::open(candidate, reason);
        if (engine_) {
            loaded_from_ = candidate.library;
            return engine_.get();
        }
        messages += (messages.empty() ? "" : "; ") + reason;
    }
    error = messages.empty() ? std::string("No ngspice library location is known.") : messages;
    return nullptr;
}

std::filesystem::path EngineHost::library_path() const
{
    std::lock_guard lock(state_);
    return loaded_from_;
}

} // namespace simnodus::sim
