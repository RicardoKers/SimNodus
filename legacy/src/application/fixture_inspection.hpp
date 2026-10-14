// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#pragma once
#include "core/joint_session.hpp"
#include "core/adc_fixture.hpp"
#include "core/fixture_readback.hpp"
#include "adapters/gdb/fixture_memory.hpp"
#include "adapters/gdb/fixture_time.hpp"
#include "adapters/gdb/fixture_registers.hpp"
#include <chrono>
#include <charconv>
#include <iomanip>
#include <istream>
#include <ostream>
#include <string>
#include <string_view>

namespace simnodus {
// Owns only the bounded fixture's final validation protocol. No engine transport.
class FixtureInspection {
public:
    bool permits(std::string_view verb) const noexcept
    {
        return !((inspection_step == 5 && verb != "inspection-time-result" && verb != "abort")
            || (inspection_step == 4 && verb != "release-inspection" && verb != "abort")
            || ((inspection_step == 1 || inspection_step == 2) && verb != "inspection-result" && verb != "abort")
            || (readback_pending && verb != "readback-mi" && verb != "readback-timed" && verb != "abort"));
    }
    bool commit_ready(const SessionSnapshot& s) const noexcept
    {
        return !(native_readback && s.cpu_acknowledged_ns >= FixtureReadback::boundary_ns
            && (!readback_verified || (native_inspection && !inspection_verified)));
    }
    void clear_pending() noexcept { readback_pending = false; inspection_step = 0; }
    void diagnostics(std::ostream& output, const SessionSnapshot& s) const
    {
        output << ",\"native_readback\":" << native_readback << ",\"readback_verified\":" << readback_verified;
        if(readback_correlated) {
            output << ",\"readback_pending\":" << readback_pending;
            if(readback_armed) {
                output << ",\"readback_request\":{\"token\":" << readback_token
                    << ",\"command\":\"" << readback_token << "-data-read-memory-bytes 0x20000000 32\"";
                if(native_readback_time) output << ",\"time_token\":" << readback_token + 1'000'000
                    << ",\"time_command\":" << std::quoted(std::to_string(readback_token + 1'000'000)
                        + "-interpreter-exec console \"monitor machine ElapsedVirtualTime\"");
                output << "}";
            }
        }
        if(native_inspection) output << ",\"inspection_verified\":" << inspection_verified;
        if(inspection_correlated) {
            output << ",\"inspection_step\":" << inspection_step;
            if(inspection_step != 0 && inspection_step != 4 && inspection_step != 5) {
                const auto request_token = (inspection_step == 1 ? 3'000'000 : 4'000'000) + s.generation;
                output << ",\"inspection_request\":{\"token\":" << request_token
                    << ",\"command\":\"" << request_token << "-data-list-register-values x 0 13 14\"}";
            }
        }
        if(inspection_interval) {
            output << ",\"inspection_interval_ns\":" << inspection_interval_ns;
            if(inspection_step == 4) {
                const auto left = inspection_started + std::chrono::milliseconds(100) - std::chrono::steady_clock::now();
                const auto wait_ms = left > std::chrono::steady_clock::duration::zero()
                    ? std::chrono::ceil<std::chrono::milliseconds>(left).count() : 0;
                output << ",\"inspection_wait_ms\":" << wait_ms;
            }
        }
        if(inspection_time && (inspection_step == 5 || inspection_step == 3)) {
            const auto time_token = 5'000'000 + s.generation;
            output << ",\"inspection_time_request\":{\"token\":" << time_token
                << ",\"command\":" << std::quoted(std::to_string(time_token)
                    + "-interpreter-exec console \"monitor machine ElapsedVirtualTime\"") << "}";
        }
    }
    bool command(std::string_view verb, std::istream& input, JointSession& session,
        bool activation_allowed, bool adc_prepared, bool adc_pending,
        unsigned adc_confirmations, const AdcFixtureInput& adc_input)
    {
        std::string token;
        if(verb == "arm-inspection") {
            if(!inspection_correlated || inspection_step != 0 || !readback_verified
                || session.snapshot().phase != SessionPhase::analog_ready
                || session.snapshot().cpu_acknowledged_ns != FixtureReadback::boundary_ns
                || std::chrono::steady_clock::now() >= readback_deadline || (input >> token))
                session.abort(SessionError::order);
            else inspection_step = 1;
        } else if(verb == "release-inspection") {
            const auto now = std::chrono::steady_clock::now();
            if(!inspection_interval || inspection_step != 4 || session.snapshot().phase != SessionPhase::analog_ready
                || now >= readback_deadline || (input >> token)) session.abort(SessionError::order);
            else if(now >= inspection_started + std::chrono::milliseconds(100)) {
                inspection_interval_ns = static_cast<Nanoseconds>(std::chrono::duration_cast<std::chrono::nanoseconds>(now-inspection_started).count());
                inspection_step = 2;
            }
        } else if(verb == "inspection-result") {
            std::string raw;
            std::array<std::uint32_t, 3> values{};
            const auto expected_token = (inspection_step == 1 ? 3'000'000 : 4'000'000) + session.snapshot().generation;
            if(!inspection_correlated || (inspection_step != 1 && inspection_step != 2)
                || session.snapshot().phase != SessionPhase::analog_ready
                || std::chrono::steady_clock::now() >= readback_deadline
                || !(input >> std::quoted(raw)) || (input >> token)
                || !gdb::parse_fixture_registers(raw, values, expected_token)
                || (inspection_step == 2 && values != inspection_first)
                || std::chrono::steady_clock::now() >= readback_deadline)
                session.abort(SessionError::order);
            else if(inspection_step == 1) {
                inspection_first = values;
                inspection_started = std::chrono::steady_clock::now();
                inspection_step = inspection_interval ? 4 : 2;
            }
            else if(inspection_time) inspection_step = 5;
            else { inspection_step = 3; inspection_verified = true; }
        } else if(verb == "inspection-time-result") {
            std::string stream, completion;
            Nanoseconds observed = 0;
            if(!inspection_time || inspection_step != 5 || session.snapshot().phase != SessionPhase::analog_ready
                || std::chrono::steady_clock::now() >= readback_deadline
                || !(input >> std::quoted(stream) >> std::quoted(completion)) || (input >> token)
                || !gdb::parse_fixture_time(stream, completion, 5'000'000 + session.snapshot().generation, observed)
                || observed != FixtureReadback::boundary_ns || observed != session.snapshot().cpu_acknowledged_ns
                || std::chrono::steady_clock::now() >= readback_deadline)
                session.abort(SessionError::order);
            else { inspection_step = 3; inspection_verified = true; }
        } else if(verb == "verify-inspection") {
            std::string before, after;
            std::array<std::uint32_t, 3> first{}, second{};
            if(!native_inspection || inspection_correlated || inspection_verified || !readback_verified
                || session.snapshot().phase != SessionPhase::analog_ready
                || session.snapshot().cpu_acknowledged_ns != FixtureReadback::boundary_ns
                || std::chrono::steady_clock::now() >= readback_deadline
                || !(input >> std::quoted(before) >> std::quoted(after)) || (input >> token)
                || !gdb::parse_fixture_registers(before, first) || !gdb::parse_fixture_registers(after, second)
                || first != second || std::chrono::steady_clock::now() >= readback_deadline)
                session.abort(SessionError::order);
            else inspection_verified = true;
        } else if(verb == "arm-readback") {
            if(!readback_correlated || readback_armed || readback_verified || !adc_prepared || adc_pending || adc_confirmations != 1
                || session.snapshot().phase != SessionPhase::analog_ready
                || session.snapshot().cpu_acknowledged_ns != FixtureReadback::boundary_ns || (input >> token))
                session.abort(SessionError::order);
            else {
                // Reserved MI token range, used once in this owned runner/GDB session.
                readback_token = 1'000'000 + session.snapshot().generation;
                readback_armed = readback_pending = true;
                readback_deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(1900);
            }
        } else if(verb == "readback-mi" || verb == "readback-timed") {
            Nanoseconds time = 0;
            std::string raw, time_text, completion;
            std::array<Nanoseconds, 8> words{};
            const bool timed = verb == "readback-timed";
            bool fields = false;
            if(timed) {
                fields = static_cast<bool>(input >> std::quoted(raw) >> std::quoted(time_text) >> std::quoted(completion))
                    && !(input >> token) && gdb::parse_fixture_time(time_text, completion, readback_token + 1'000'000, time);
            } else {
                fields = static_cast<bool>(input >> time_text >> std::quoted(raw)) && !(input >> token);
                const auto [time_end, time_error] = std::from_chars(time_text.data(), time_text.data()+time_text.size(), time);
                fields = fields && time_error == std::errc{} && time_end == time_text.data()+time_text.size();
            }
            if((readback_correlated && (!readback_pending || std::chrono::steady_clock::now() >= readback_deadline))
                || timed != native_readback_time || !native_readback_mi || readback_verified || !adc_prepared || adc_pending || adc_confirmations != 1
                || session.snapshot().phase != SessionPhase::analog_ready
                || session.snapshot().cpu_acknowledged_ns != FixtureReadback::boundary_ns
                || !fields
                || !gdb::parse_fixture_memory(raw, words, readback_correlated ? readback_token : 91)
                || !FixtureReadback::accepts(time, words, adc_input.expected_code)
                || (readback_correlated && std::chrono::steady_clock::now() >= readback_deadline))
                session.abort(SessionError::order);
            else { readback_verified = true; readback_pending = false; }
        } else if(verb == "enable-readback" || verb == "enable-readback-mi" || verb == "enable-readback-request" || verb == "enable-readback-time" || verb == "enable-inspection" || verb == "enable-inspection-request" || verb == "enable-inspection-interval" || verb == "enable-inspection-time") {
            if(!activation_allowed || native_readback
                || session.snapshot().phase != SessionPhase::stopped || session.snapshot().commits != 0
                || (input >> token)) session.abort(SessionError::order);
            else { native_readback = true; native_readback_mi = verb != "enable-readback"; inspection_time = verb == "enable-inspection-time"; inspection_interval = inspection_time || verb == "enable-inspection-interval"; inspection_correlated = inspection_interval || verb == "enable-inspection-request"; native_inspection = inspection_correlated || verb == "enable-inspection"; native_readback_time = native_inspection || verb == "enable-readback-time"; readback_correlated = native_readback_time || verb == "enable-readback-request"; }
        } else return false;
        return true;
    }
    void normalized(std::span<const Nanoseconds> values, JointSession& session,
        bool adc_prepared, bool adc_pending, unsigned adc_confirmations,
        const AdcFixtureInput& adc_input)
    {
                if(!native_readback || native_readback_mi || readback_verified || !adc_prepared || adc_pending || adc_confirmations != 1
                    || session.snapshot().phase != SessionPhase::analog_ready
                    || session.snapshot().cpu_acknowledged_ns != FixtureReadback::boundary_ns
                    || values.size() != 9
                    || !FixtureReadback::accepts(values[0], std::span<const Nanoseconds>(values).subspan(1), adc_input.expected_code))
                    session.abort(SessionError::order);
                else readback_verified = true;
    }
private:
    bool native_readback_time = false;
    bool inspection_correlated = false, inspection_interval = false, inspection_time = false;
    std::chrono::steady_clock::time_point inspection_started;
    Nanoseconds inspection_interval_ns = 0;
    unsigned inspection_step = 0;
    std::array<std::uint32_t, 3> inspection_first{};
    bool native_inspection = false, inspection_verified = false;
    bool readback_correlated = false, readback_pending = false, readback_armed = false;
    Nanoseconds readback_token = 0;
    std::chrono::steady_clock::time_point readback_deadline;
    bool native_readback = false, readback_verified = false, native_readback_mi = false;
};
} // namespace simnodus
