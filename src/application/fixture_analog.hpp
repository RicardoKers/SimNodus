// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#pragma once
#include "core/joint_session.hpp"
#include "core/rc_trajectory.hpp"
#include "adapters/ngspice/worker_channel.hpp"
#include "adapters/ngspice/worker_reply.hpp"
#include <iomanip>
#include <istream>
#include <ostream>
#include <string>
#include <string_view>

namespace simnodus {
// Bounded worker sequencing; endpoints are borrowed and must outlive this object.
class FixtureAnalog {
public:
    FixtureAnalog(ngspice::WorkerChannel* channel, ngspice::WorkerReader* reader)
        : analog_channel(channel), worker_reader(reader) {}
    FixtureAnalog(const FixtureAnalog&) = delete;
    FixtureAnalog& operator=(const FixtureAnalog&) = delete;
    bool permits(std::string_view verb) const noexcept
    { return !worker_pending || verb == "poll-worker" || verb == "abort"; }
    bool adc_allowed() const noexcept { return worker_reader && worker_initialized && high_writes == 1; }
    bool inspection_activation_allowed() const noexcept { return worker_reader && !worker_initialized && !worker_pending; }
    const AnalogObservation& observation() const noexcept { return worker_last.analog; }
    void begin_command() noexcept { worker_reply = false; }
    void clear_pending() noexcept { worker_pending = false; }
    void diagnostics(std::ostream& output, const SessionSnapshot& s) const
    {
        output << ",\"native_rc\":" << native_rc << ",\"rc_checks\":" << rc_checks;
        if(analog_channel) output << ",\"advance_writes\":" << advance_writes
            << ",\"analog_pending\":" << analog_pending
            << ",\"high_writes\":" << high_writes << ",\"inspect_writes\":" << inspect_writes;
        if(worker_reader) {
            output << ",\"worker_status\":\"" << ngspice::worker_status_name(worker_reader->status()) << "\"";
            if(worker_reply && s.error == SessionError::none) {
                output << ",\"worker_result\":" << worker_reader->raw()
                    << ",\"worker_raw\":" << std::quoted(worker_reader->raw());
            }
        }
    }
    bool command(std::string_view verb, std::istream& input, JointSession& session)
    {
        std::string token;
        if(verb == "high-native" || verb == "inspect-native") {
            const auto& snapshot = session.snapshot();
            const bool high = verb == "high-native";
            const bool phase_ok = snapshot.phase == SessionPhase::analog_ready
                || (!high && snapshot.phase == SessionPhase::stopped);
            const auto expected = snapshot.phase == SessionPhase::stopped
                ? snapshot.committed_ns : snapshot.cpu_acknowledged_ns;
            // Only the measured fixture's single GPIO edge; not arbitrary live coupling.
            if(!worker_reader || !analog_channel || !worker_initialized || worker_pending || analog_pending
                || !phase_ok || worker_last.analog.requested_ns != expected
                || (high && (high_writes != 0 || expected != 2'011'375))
                || (input >> token) || !worker_reader->arm(false)) session.abort(SessionError::transport);
            else if(!(high ? analog_channel->high() : analog_channel->inspect())) session.abort(SessionError::transport);
            else {
                worker_pending = true;
                if(high) ++high_writes; else ++inspect_writes;
            }
        } else if(verb == "read-worker") {
            if(!worker_reader || worker_pending || analog_pending || session.snapshot().phase == SessionPhase::failed
                || (input >> token) || !worker_reader->arm(true)) session.abort(SessionError::transport);
            else worker_pending = true;
        } else if(verb == "enable-rc") {
            if(!worker_reader || native_rc || worker_initialized || worker_pending
                || session.snapshot().phase != SessionPhase::stopped || session.snapshot().commits != 0
                || (input >> token)) session.abort(SessionError::order);
            else native_rc = true;
        } else if(verb == "poll-worker") {
            if(!worker_reader || !worker_pending || session.snapshot().phase == SessionPhase::failed || (input >> token))
                session.abort(SessionError::transport);
            else {
                const auto status = worker_reader->poll();
                if(status == ngspice::WorkerStatus::ready) {
                    const auto& value = worker_reader->result();
                    bool valid = true;
                    if(analog_pending) {
                        valid = value.samples > worker_last.samples;
                        if(valid && native_rc) {
                            valid = RcTrajectory::accepts(value.analog, high_writes == 1);
                            if(valid) ++rc_checks;
                        }
                        if(valid) valid = session.analog(value.analog) == SessionError::none;
                    } else if(!worker_initialized) {
                        valid = value.analog.requested_ns == 0 && value.analog.observed_seconds == 0
                            && value.analog.output_volts == 0 && value.samples == 0;
                    } else valid = value.analog.requested_ns == worker_last.analog.requested_ns
                        && value.analog.observed_seconds == worker_last.analog.observed_seconds
                        && value.analog.output_volts == worker_last.analog.output_volts && value.samples == worker_last.samples;
                    if(!valid) session.abort(SessionError::analog);
                    else {
                        worker_last = value; worker_initialized = worker_reply = true;
                        worker_pending = analog_pending = false;
                    }
                } else if(status != ngspice::WorkerStatus::pending) session.abort(SessionError::transport);
            }
        } else if(verb == "advance-native") {
            if(!analog_channel || analog_pending || (worker_reader && (!worker_initialized || worker_pending)) || session.snapshot().phase != SessionPhase::cpu_acknowledged
                || (input >> token) || (worker_reader && !worker_reader->arm(false)) || !analog_channel->advance(session.snapshot().cpu_acknowledged_ns))
                session.abort(SessionError::transport);
            else {
                analog_pending = true;
                worker_pending = static_cast<bool>(worker_reader);
                ++advance_writes;
                analog_deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(1900);
            }
        } else if(verb == "analog") {
            Nanoseconds ns;
            double seconds, volts;
            if(worker_reader || (analog_channel && (!analog_pending || std::chrono::steady_clock::now() >= analog_deadline)))
                session.abort(SessionError::transport);
            else if((input >> ns >> seconds >> volts) && !(input >> token)) {
                if(session.analog({ns, seconds, volts}) == SessionError::none) analog_pending = false;
            }
            else session.abort(SessionError::analog);
        } else return false;
        return true;
    }
private:
    ngspice::WorkerChannel* const analog_channel;
    ngspice::WorkerReader* const worker_reader;
    bool worker_pending = false, worker_initialized = false, worker_reply = false;
    ngspice::WorkerReply worker_last;
    bool analog_pending = false, native_rc = false;
    unsigned rc_checks = 0, advance_writes = 0, high_writes = 0, inspect_writes = 0;
    std::chrono::steady_clock::time_point analog_deadline;
};
} // namespace simnodus
