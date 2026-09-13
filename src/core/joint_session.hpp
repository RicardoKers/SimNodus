// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#pragma once
#include "backend_contracts.hpp"
#include <limits>
#include <span>

namespace simnodus {
enum class SessionPhase { stopped, granted, observed, cpu_acknowledged, analog_ready, failed };
enum class SessionError { none, capability, order, grant, observation, accounting, analog, aborted, transport };
struct CpuOutcome {
    Nanoseconds start, end, requested, unused;
    bool cancelled;
    std::span<const Nanoseconds> sinks;
};
struct SessionSnapshot {
    SessionPhase phase = SessionPhase::stopped;
    Nanoseconds generation = 0, granted_ns = 0, observed_ns = 0;
    Nanoseconds cpu_acknowledged_ns = 0, committed_ns = 0, discarded_ns = 0;
    Nanoseconds acknowledgements = 0, commits = 0;
    SessionError error = SessionError::none;
};
// Bounded ADR 0014 fixture only. Backend transport and scheduling stay outside.
// Failure is terminal; recovery constructs a new session with fresh engines.
class JointSession {
public:
    explicit JointSession(bool cooperative_fixture = false) : enabled_(cooperative_fixture) {}
    const SessionSnapshot& snapshot() const noexcept { return state_; }
    SessionError abort(SessionError error = SessionError::aborted) noexcept
    {
        if(state_.phase != SessionPhase::failed) {
            state_.phase = SessionPhase::failed;
            state_.error = error;
        }
        return state_.error;
    }
    SessionError begin(Nanoseconds duration, bool paced) noexcept
    {
        if(state_.phase != SessionPhase::stopped) return abort(SessionError::order);
        if(!enabled_ || (duration == 100'000'000 && !paced)) return abort(SessionError::capability);
        if((duration != 5'000'000 && duration != 100'000'000)
            || duration > std::numeric_limits<Nanoseconds>::max() - state_.committed_ns)
            return abort(SessionError::grant);
        state_.granted_ns = duration;
        state_.observed_ns = state_.committed_ns;
        ++state_.generation;
        state_.phase = SessionPhase::granted;
        return SessionError::none;
    }
    SessionError observe(Nanoseconds time) noexcept
    {
        if(state_.phase != SessionPhase::granted && state_.phase != SessionPhase::observed)
            return abort(SessionError::order);
        if(time < state_.observed_ns || time - state_.committed_ns > state_.granted_ns)
            return abort(SessionError::observation);
        state_.observed_ns = time;
        state_.phase = SessionPhase::observed;
        return SessionError::none;
    }
    SessionError acknowledge(const CpuOutcome& v) noexcept
    {
        if(state_.phase != SessionPhase::observed) return abort(SessionError::order);
        // Subtraction after bounds checks avoids accepting overflowing sums.
        if(v.start != state_.committed_ns || v.end < v.start || v.end < state_.observed_ns
            || v.requested != state_.granted_ns || v.unused > v.requested
            || v.end - v.start != v.requested - v.unused
            || !v.cancelled || v.unused == 0 || v.end == v.start || v.sinks.size() != 1
            || v.sinks.front() != v.end)
            return abort(SessionError::accounting);
        state_.cpu_acknowledged_ns = v.end;
        state_.discarded_ns = v.unused;
        ++state_.acknowledgements;
        state_.phase = SessionPhase::cpu_acknowledged;
        return SessionError::none;
    }
    SessionError analog(const AnalogObservation& value) noexcept
    {
        if(state_.phase != SessionPhase::cpu_acknowledged) return abort(SessionError::order);
        if(value.requested_ns != state_.cpu_acknowledged_ns
            || RcFixtureContract::observation(value) != BoundaryError::none)
            return abort(SessionError::analog);
        state_.phase = SessionPhase::analog_ready;
        return SessionError::none;
    }
    // Called only after fixture exchange/inspection succeeds. Agreement alone
    // must not publish a checkpoint or debugger notification.
    SessionError commit() noexcept
    {
        if(state_.phase != SessionPhase::analog_ready) return abort(SessionError::order);
        state_.committed_ns = state_.cpu_acknowledged_ns;
        ++state_.commits;
        state_.phase = SessionPhase::stopped;
        return SessionError::none;
    }
private:
    bool enabled_;
    SessionSnapshot state_;
};
} // namespace simnodus
