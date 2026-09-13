// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#pragma once
#include "core/joint_session.hpp"
#include "adapters/renode/cancellation_result.hpp"
#include "adapters/renode/control_channel.hpp"
#include <charconv>
#include <iomanip>
#include <memory>
#include <ostream>
#include <sstream>
#include <string>

namespace simnodus {
// Execution and debug transitions share deadlines; the CLI owns the channel/session.
class FixtureExecution {
public:
    explicit FixtureExecution(renode::ControlChannel* endpoint) : channel(endpoint) {}
    FixtureExecution(const FixtureExecution&) = delete;
    FixtureExecution& operator=(const FixtureExecution&) = delete;
    void suppress_reply() noexcept { transport_reply = false; }
    bool commit_ready() const noexcept { return !notify_pending; }
    void begin(JointSession& session, Nanoseconds duration, bool paced)
    {
        if(session.begin(duration, paced) == SessionError::none) {
            native_ready = native_started = false;
            sample_pending = notify_pending = notification_sent = false;
            debug_reader.reset();
            progress_deadline.reset();
        }
    }
    void observe(JointSession& session, Nanoseconds observed)
    {
        if((channel && !native_ready) || (debug_reader && sample_pending)) session.abort(SessionError::transport);
        else if(session.observe(observed) == SessionError::none) sample_pending = false;
    }
    void diagnostics(std::ostream& output, const SessionSnapshot& s) const
    {
        if(transport_reply) {
            output << ",\"result_status\":\"" << renode::status_name(read_status)
                      << "\",\"read_retries\":" << (reader ? reader->retries() : 0);
            if(read_status == renode::ResultStatus::ready && s.error == SessionError::none) {
                const auto& v = reader->result();
                output << ",\"cpu_result\":{\"start\":" << v.start << ",\"end\":" << v.end
                          << ",\"requested\":" << v.requested << ",\"unused\":" << v.unused
                          << ",\"sinks\":[" << v.sink << "],\"reason\":\"cancelled\",\"cancelled\":\"True\"}";
            }
        }
        if(channel) {
            output << ",\"start_writes\":" << start_writes << ",\"cancel_writes\":" << cancel_writes
                      << ",\"sample_writes\":" << sample_writes << ",\"notify_writes\":" << notify_writes
                      << ",\"sample_pending\":" << sample_pending << ",\"notify_pending\":" << notify_pending
                      << ",\"ready_status\":\"" << renode::status_name(ready_status) << "\"";
        }
        if(debug_reader) output << ",\"debug_status\":\"" << renode::status_name(debug_status)
            << "\",\"debug_retries\":" << debug_reader->retries();
    }
    bool command(std::string verb, std::istringstream& input, JointSession& session)
    {
        std::string token;
        // Composite fixture transitions reuse the same measured transport paths.
        if(verb == "grant-native") {
            std::string duration_text, paced_text, path, command;
            Nanoseconds duration = 0;
            bool valid = static_cast<bool>(input >> duration_text >> paced_text >> std::quoted(path));
            const auto [end, error] = std::from_chars(duration_text.data(), duration_text.data()+duration_text.size(), duration);
            valid = valid && error == std::errc{} && end == duration_text.data()+duration_text.size()
                && (paced_text == "0" || paced_text == "1") && !(input >> token)
                && channel && renode::start_command(path, duration, command);
            if(!valid) session.abort(SessionError::transport);
            else if(session.begin(duration, paced_text == "1") == SessionError::none) {
                native_ready = native_started = false;
                sample_pending = notify_pending = notification_sent = false;
                debug_reader.reset();
                progress_deadline.reset();
                input.clear(); input.str("\"" + path + "\"");
                verb = "start-native";
            }
            if(verb == "grant-native") { transport_reply = false; return true; }
        } else if(verb == "stop-native") {
            std::string time_text;
            Nanoseconds observed = 0;
            bool valid = static_cast<bool>(input >> time_text);
            const auto [end, error] = std::from_chars(time_text.data(), time_text.data()+time_text.size(), observed);
            valid = valid && error == std::errc{} && end == time_text.data()+time_text.size()
                && !(input >> token) && channel && native_ready && !sample_pending
                && (!reader || reader->status() != renode::ResultStatus::pending);
            if(!valid) session.abort(SessionError::transport);
            else if(session.observe(observed) == SessionError::none) {
                input.clear(); input.str("");
                verb = "cancel-native";
            }
            if(verb == "stop-native") { transport_reply = false; return true; }
        }
        transport_reply = verb == "expect-result" || verb == "poll-result" || verb == "cancel-native";
        if(verb == "poll-debug") {
            const auto phase = session.snapshot().phase;
            if(!debug_reader || (input >> token)
                || (debug_notification ? !notify_pending || phase != SessionPhase::analog_ready
                    : !sample_pending || (phase != SessionPhase::granted && phase != SessionPhase::observed)))
                session.abort(SessionError::transport);
            else {
                debug_status = debug_reader->poll();
                if(debug_status == renode::ResultStatus::ready) {
                    if(debug_notification) notify_pending = false;
                    else if(session.observe(debug_reader->observed()) == SessionError::none) sample_pending = false;
                } else if(debug_status != renode::ResultStatus::pending) session.abort(SessionError::transport);
            }
        } else if(verb == "sample-native" || verb == "sample-ingress") {
            std::string path;
            const auto phase = session.snapshot().phase;
            if(!channel || !native_ready || sample_pending
                || (phase != SessionPhase::granted && phase != SessionPhase::observed)
                || (reader && reader->status() == renode::ResultStatus::pending)
                || !(input >> std::quoted(path)) || (input >> token))
                session.abort(SessionError::transport);
            else {
                debug_reader.reset();
                if(verb == "sample-ingress") {
                    debug_notification = false;
                    if(!progress_deadline) progress_deadline = std::make_unique<renode::ResultDeadline>(
                        std::chrono::steady_clock::now(), std::chrono::milliseconds(1900));
                    debug_reader = std::make_unique<renode::DebugReplyReader>(path, false, *progress_deadline);
                    debug_status = debug_reader->status();
                }
                if((debug_reader && debug_status != renode::ResultStatus::pending) || !channel->sample(path))
                    session.abort(SessionError::transport);
                else { sample_pending = true; ++sample_writes; }
            }
        } else if(verb == "notify-native" || verb == "notify-ingress") {
            std::string path;
            if(!channel || notification_sent || session.snapshot().phase != SessionPhase::analog_ready
                || std::chrono::steady_clock::now() >= pause_deadline
                || !(input >> std::quoted(path)) || (input >> token))
                session.abort(SessionError::transport);
            else {
                debug_reader.reset();
                if(verb == "notify-ingress") {
                    debug_notification = true;
                    debug_reader = std::make_unique<renode::DebugReplyReader>(path, true,
                        renode::ResultDeadline(pause_deadline, std::chrono::milliseconds(0)));
                    debug_status = debug_reader->status();
                }
                if((debug_reader && debug_status != renode::ResultStatus::pending)
                    || !channel->notify(path, session.snapshot().cpu_acknowledged_ns)) session.abort(SessionError::transport);
                else { notify_pending = true; notification_sent = true; ++notify_writes; }
            }
        } else if(verb == "notified") {
            // Host attests the bridge token; native response ingress is a subsequent slice.
            if(debug_reader || !channel || !notify_pending || session.snapshot().phase != SessionPhase::analog_ready
                || std::chrono::steady_clock::now() >= pause_deadline || (input >> token))
                session.abort(SessionError::transport);
            else notify_pending = false;
        } else if(verb == "start-native") {
            std::string path, command;
            if(!channel || native_started || session.snapshot().phase != SessionPhase::granted
                || !(input >> std::quoted(path)) || (input >> token)
                || !renode::start_command(path, session.snapshot().granted_ns, command)) {
                session.abort(SessionError::transport);
            } else {
                grant_path = path;
                ready_reader = std::make_unique<renode::ReadyReader>(std::filesystem::path(path), std::chrono::milliseconds(1900));
                ready_status = ready_reader->status();
                if(ready_status != renode::ResultStatus::pending || !channel->start(path, session.snapshot().granted_ns))
                    session.abort(SessionError::transport);
                else { native_started = true; ++start_writes; }
            }
        } else if(verb == "poll-ready") {
            if(!channel || !native_started || native_ready || !ready_reader || (input >> token)
                || session.snapshot().phase != SessionPhase::granted) session.abort(SessionError::transport);
            else {
                ready_status = ready_reader->poll();
                if(ready_status == renode::ResultStatus::ready) native_ready = true;
                else if(ready_status != renode::ResultStatus::pending) session.abort(SessionError::transport);
            }
        } else if(verb == "cancel-native") {
            if(!channel || !native_ready || sample_pending || session.snapshot().phase != SessionPhase::observed
                || (reader && reader->status() == renode::ResultStatus::pending) || (input >> token))
                session.abort(SessionError::transport);
            else {
                reader = std::make_unique<renode::CancellationReader>(std::filesystem::path(grant_path), std::chrono::milliseconds(1900));
                read_status = reader->status();
                if(read_status != renode::ResultStatus::pending || !channel->cancel()) session.abort(SessionError::transport);
                else {
                    ++cancel_writes;
                    pause_deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(1900);
                }
            }
        } else if(verb == "expect-result") {
            std::string path;
            unsigned budget = 0;
            if(channel || session.snapshot().phase != SessionPhase::observed
                || (reader && reader->status() == renode::ResultStatus::pending)
                || !(input >> std::quoted(path) >> budget) || (input >> token)) {
                read_status = renode::ResultStatus::io_error;
                session.abort(SessionError::transport);
            } else {
                reader = std::make_unique<renode::CancellationReader>(std::filesystem::path(std::u8string(path.begin(), path.end())),
                    std::chrono::milliseconds(budget));
                read_status = reader->status();
                if(read_status != renode::ResultStatus::pending) session.abort(SessionError::transport);
            }
        } else if(verb == "poll-result") {
            if(!reader || (input >> token) || session.snapshot().phase != SessionPhase::observed) {
                read_status = renode::ResultStatus::consumed;
                session.abort(SessionError::transport);
            } else {
                read_status = reader->poll();
                if(read_status == renode::ResultStatus::ready) session.acknowledge(reader->result().outcome());
                else if(read_status != renode::ResultStatus::pending) session.abort(SessionError::transport);
            }
        } else return false;
        return true;
    }
private:
    renode::ControlChannel* const channel;
    std::unique_ptr<renode::ReadyReader> ready_reader;
    bool native_ready = false, native_started = false;
    std::string grant_path;
    unsigned start_writes = 0, cancel_writes = 0, sample_writes = 0, notify_writes = 0;
    bool sample_pending = false, notify_pending = false, notification_sent = false;
    std::chrono::steady_clock::time_point pause_deadline;

    std::unique_ptr<renode::DebugReplyReader> debug_reader;
    bool debug_notification = false;
    std::unique_ptr<renode::ResultDeadline> progress_deadline;
    renode::ResultStatus debug_status = renode::ResultStatus::pending;
    renode::ResultStatus ready_status = renode::ResultStatus::pending;
    std::unique_ptr<renode::CancellationReader> reader;
    bool transport_reply = false;
    renode::ResultStatus read_status = renode::ResultStatus::pending;
};
} // namespace simnodus
