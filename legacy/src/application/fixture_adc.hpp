// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#pragma once
#include "core/joint_session.hpp"
#include "core/adc_fixture.hpp"
#include "adapters/renode/adc_process.hpp"
#include <charconv>
#include <iomanip>
#include <istream>
#include <ostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace simnodus {
// Bounded ADC transfer and owned helper lifetime; no simulation time authority.
class FixtureAdc {
public:
    FixtureAdc(std::filesystem::path executable, unsigned port)
        : adc_executable(std::move(executable)), adc_port(port) {}
    bool prepared() const noexcept { return adc_prepared; }
    bool pending() const noexcept { return adc_pending; }
    unsigned confirmations() const noexcept { return adc_confirmations; }
    const AdcFixtureInput& input() const noexcept { return adc_input; }
    bool permits(std::string_view verb) const noexcept
    {
        return !adc_pending || verb == "adc-applied" || verb == "apply-adc"
            || verb == "poll-adc" || verb == "abort";
    }
    void cancel()
    {
        adc_pending = false;
        if(adc_process) adc_process->cancel();
    }
    void diagnostics(std::ostream& output, const SessionSnapshot& s) const
    {
        if(adc_prepared) output << ",\"adc_pending\":" << adc_pending
            << ",\"adc_confirmations\":" << adc_confirmations
            << ",\"adc_request\":{\"channel\":0,\"microvolts\":" << adc_input.microvolts
            << ",\"expected_code\":" << adc_input.expected_code << ",\"time_ns\":" << AdcFixtureInput::boundary_ns << "}";
        if(adc_process) {
            output << ",\"adc_process_status\":\"" << adc_process->status() << "\""
                << ",\"adc_helper_pid\":" << adc_process->pid()
                << ",\"adc_helper_exited\":" << adc_process->exited()
                << ",\"adc_helper_exit\":" << adc_process->exit_code()
                << ",\"adc_helper_cleaned\":" << adc_process->cleaned()
                << ",\"adc_stdout_hex\":\"" << adc_process->stdout_hex() << "\""
                << ",\"adc_stderr_hex\":\"" << adc_process->stderr_hex() << "\"";
            if(adc_confirmations && s.error == SessionError::none)
                output << ",\"adc_control\":{\"command\":\"adc\",\"before_us\":4010,\"after_us\":4010}";
        }
    }
    bool command(std::string_view verb, std::istream& input, JointSession& session,
        bool preparation_allowed, const AnalogObservation& sample)
    {
        std::string token;
        if(verb == "prepare-adc") {
            if(!preparation_allowed || adc_prepared
                || session.snapshot().phase != SessionPhase::analog_ready
                || session.snapshot().cpu_acknowledged_ns != AdcFixtureInput::boundary_ns
                || sample.requested_ns != AdcFixtureInput::boundary_ns
                || (input >> token) || !AdcFixtureInput::prepare(sample.output_volts, adc_input))
                session.abort(SessionError::transport);
            else {
                adc_prepared = adc_pending = true;
                adc_deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(1900);
            }
        } else if(verb == "apply-adc") {
            if(adc_executable.empty() || !adc_pending || adc_process
                || session.snapshot().phase != SessionPhase::analog_ready || (input >> token)) session.abort(SessionError::transport);
            else {
                adc_process = std::make_unique<renode::AdcProcess>();
                if(!adc_process->start(adc_executable, adc_port, adc_input.microvolts, adc_deadline)) {
                    adc_process->cancel(); session.abort(SessionError::transport);
                }
            }
        } else if(verb == "poll-adc") {
            if(!adc_process || !adc_pending || session.snapshot().phase != SessionPhase::analog_ready || (input >> token))
                session.abort(SessionError::transport);
            else {
                const std::string_view status(adc_process->poll());
                if(status == "ready") { adc_pending = false; ++adc_confirmations; }
                else if(status != "pending") session.abort(SessionError::transport);
            }
        } else if(verb == "adc-applied") {
            std::vector<Nanoseconds> fields;
            bool valid = true;
            while(input >> token) {
                Nanoseconds value = 0;
                const auto [end, error] = std::from_chars(token.data(), token.data()+token.size(), value);
                if(error != std::errc{} || end != token.data()+token.size()) { valid = false; break; }
                fields.push_back(value);
                if(fields.size() > 4) { valid = false; break; }
            }
            // The existing control helper reports microsecond time, not nanoseconds.
            if(!adc_executable.empty() || !adc_pending || !valid || fields.size() != 4
                || session.snapshot().phase != SessionPhase::analog_ready
                || std::chrono::steady_clock::now() >= adc_deadline
                || fields[0] != AdcFixtureInput::channel || fields[1] != adc_input.microvolts
                || fields[2] != AdcFixtureInput::boundary_ns / 1000 || fields[3] != fields[2])
                session.abort(SessionError::transport);
            else { adc_pending = false; ++adc_confirmations; }
        } else return false;
        return true;
    }
private:
    const std::filesystem::path adc_executable;
    const unsigned adc_port;
    std::unique_ptr<renode::AdcProcess> adc_process;
    AdcFixtureInput adc_input;
    bool adc_prepared = false, adc_pending = false;
    unsigned adc_confirmations = 0;
    std::chrono::steady_clock::time_point adc_deadline;
};
} // namespace simnodus
