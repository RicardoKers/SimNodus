// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "application/fixture_session.hpp"
#include <windows.h>
#include <iostream>
#include <type_traits>
static_assert(!std::is_copy_constructible_v<simnodus::FixtureSession>);
int main()
{
    using namespace simnodus;
    using Result = FixtureSession::CommandResult;
    unsigned checks = 0, failures = 0;
    auto check = [&](bool ok) { ++checks; if(!ok) ++failures; };
    FixtureSession first(nullptr, nullptr, nullptr), second(nullptr, nullptr, nullptr);
    check(first.command("quit") == Result::exit_success);
    check(first.command("begin 5000000 0") == Result::reply);
    check(first.command("quit") == Result::exit_failure);
    check(first.snapshot().phase == SessionPhase::granted && second.snapshot().phase == SessionPhase::stopped);
    first.command("observe 2010875");
    first.command("ack 0 2010875 5000000 2989125 1 2010875");
    first.command("analog 2010875 0.002010875 0");
    check(first.snapshot().phase == SessionPhase::analog_ready && first.snapshot().commits == 0);
    first.command("commit");
    check(first.snapshot().commits == 1 && second.snapshot().commits == 0);
    first.command("unknown");
    check(first.snapshot().phase == SessionPhase::failed && first.snapshot().commits == 1);
    first.command("begin 5000000 0"); first.command("commit");
    check(first.snapshot().commits == 1 && first.command("quit") == Result::exit_success);
    for(const char* rejected : {"commit", "quit", "begin 5000000 0", "analog 0 0 0"}) {
        HANDLE read_handle = nullptr, write_handle = nullptr;
        if(!CreatePipe(&read_handle, &write_handle, nullptr, 0)) return 1;
        {
            ngspice::WorkerReader reader(reinterpret_cast<std::uintptr_t>(read_handle));
            FixtureSession pending(nullptr, nullptr, &reader);
            check(pending.command("read-worker") == Result::reply && reader.status() == ngspice::WorkerStatus::pending);
            check(pending.command(rejected) == Result::reply);
            check(pending.snapshot().phase == SessionPhase::failed && pending.snapshot().error == SessionError::transport
                && pending.snapshot().commits == 0 && pending.snapshot().generation == 0);
            pending.command("abort");
            check(pending.command("quit") == Result::exit_success);
        }
        CloseHandle(write_handle);
    }
    std::cout << checks << " application session checks, failures=" << failures << '\n';
    return failures ? 1 : 0;
}
