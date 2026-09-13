// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#pragma once
#include "application/fixture_inspection.hpp"
#include "application/fixture_adc.hpp"
#include "application/fixture_analog.hpp"
#include "application/fixture_execution.hpp"
#include <ostream>
#include <sstream>
#include <vector>

namespace simnodus {
// One bounded protocol session. Borrowed endpoints must outlive the session.
class FixtureSession {
public:
    enum class CommandResult { reply, exit_success, exit_failure };
    FixtureSession(renode::ControlChannel* cpu, ngspice::WorkerChannel* worker,
        ngspice::WorkerReader* reader, std::filesystem::path helper = {}, unsigned port = 0)
        : channel(cpu), adc(std::move(helper), port), analog(worker, reader), execution(cpu) {}
    FixtureSession(const FixtureSession&) = delete;
    FixtureSession& operator=(const FixtureSession&) = delete;
    const SessionSnapshot& snapshot() const noexcept { return session.snapshot(); }
    void diagnostics(std::ostream& output) const
    {
        const auto& s = session.snapshot();
        output << "{\"phase\":" << static_cast<int>(s.phase)
                  << ",\"error\":" << static_cast<int>(s.error)
                  << ",\"generation\":" << s.generation << ",\"granted_ns\":" << s.granted_ns
                  << ",\"observed_ns\":" << s.observed_ns
                  << ",\"cpu_acknowledged_ns\":" << s.cpu_acknowledged_ns
                  << ",\"committed_ns\":" << s.committed_ns << ",\"discarded_ns\":" << s.discarded_ns
                  << ",\"acknowledgements\":" << s.acknowledgements
                  << ",\"commits\":" << s.commits;
        execution.diagnostics(output, s);
        analog.diagnostics(output, s);
        adc.diagnostics(output, s);
        inspection.diagnostics(output, s);
        output << "}" << std::endl;
    }
    CommandResult command(const std::string& line)
    {
        std::istringstream input(line);
        std::string verb, token;
        input >> verb;
        analog.begin_command();
        if(!inspection.permits(verb)
            || !analog.permits(verb)
            || !adc.permits(verb)) {
            session.abort(SessionError::transport); execution.suppress_reply(); return CommandResult::reply;
        }
        if(verb == "quit" && !(input >> token))
            return session.snapshot().phase == SessionPhase::stopped
                || session.snapshot().phase == SessionPhase::failed ? CommandResult::exit_success : CommandResult::exit_failure;
        if(execution.command(verb, input, session)) {
            // Execution and debugger transitions share one coordinator.
        } else if(adc.command(verb, input, session,
            analog.adc_allowed(), analog.observation())) {
            // The ADC coordinator owns transfer state and helper lifetime.
        } else if(inspection.command(verb, input, session,
            analog.inspection_activation_allowed(),
            adc.prepared(), adc.pending(), adc.confirmations(), adc.input())) {
            // Final validation state and protocol are owned by the coordinator.
        } else if(analog.command(verb, input, session)) {
            // The analog coordinator owns worker sequencing and acceptance.
        } else {
            std::vector<Nanoseconds> values;
            bool valid = true;
            while(input >> token) {
                Nanoseconds value = 0;
                const auto [end, error] = std::from_chars(token.data(), token.data()+token.size(), value);
                if(error != std::errc{} || end != token.data()+token.size()) { valid = false; break; }
                values.push_back(value);
            }
            if(!valid) session.abort(SessionError::grant);
            else if(verb == "begin" && values.size() == 2 && values[1] <= 1) {
                execution.begin(session, values[0], values[1] == 1);
            }
            else if(verb == "observe" && values.size() == 1) {
                execution.observe(session, values[0]);
            }
            else if(!channel && verb == "ack" && values.size() == 6 && values[4] <= 1)
                session.acknowledge({values[0], values[1], values[2], values[3], values[4] == 1,
                                     std::span<const Nanoseconds>(values.data()+5, 1)});
            else if(verb == "readback") {
                inspection.normalized(values, session, adc.prepared(), adc.pending(), adc.confirmations(), adc.input());
            }
            else if(verb == "commit" && values.empty()) {
                if(!execution.commit_ready() || !inspection.commit_ready(session.snapshot())) session.abort(SessionError::transport);
                else session.commit();
            }
            else if(verb == "abort" && values.empty()) { session.abort(); analog.clear_pending(); inspection.clear_pending(); adc.cancel(); }
            else session.abort(SessionError::order);
        }
        return CommandResult::reply;
    }
private:
    renode::ControlChannel* const channel;
    FixtureAdc adc;
    FixtureAnalog analog;
    FixtureInspection inspection;
    FixtureExecution execution;
    JointSession session{true};
};
} // namespace simnodus
