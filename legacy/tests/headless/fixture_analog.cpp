// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "application/fixture_analog.hpp"
#include <windows.h>
#include <iostream>
#include <sstream>
#include <type_traits>
static_assert(!std::is_copy_constructible_v<simnodus::FixtureAnalog>);
int main()
{
    using namespace simnodus;
    unsigned checks = 0, failures = 0;
    auto check = [&](bool ok) { ++checks; if(!ok) ++failures; };
    HANDLE read_handle = nullptr, write_handle = nullptr;
    if(!CreatePipe(&read_handle, &write_handle, nullptr, 0)) return 1;
    {
        ngspice::WorkerChannel channel(reinterpret_cast<std::uintptr_t>(write_handle));
        check(channel.valid());
        {
            FixtureAnalog first(&channel, nullptr), second(nullptr, nullptr);
            JointSession a(true), b(true);
            std::istringstream unrelated("unchanged arguments");
            check(!first.command("prepare-adc", unrelated, a));
            std::string text; std::getline(unrelated, text);
            check(text == "unchanged arguments" && a.snapshot().error == SessionError::none);
            check(!first.adc_allowed() && !second.adc_allowed());
            check(!first.inspection_activation_allowed());
            check(first.permits("commit"));
            std::istringstream empty;
            check(first.command("advance-native", empty, a));
            check(a.snapshot().phase == SessionPhase::failed && a.snapshot().commits == 0);
            check(b.snapshot().phase == SessionPhase::stopped && second.observation().requested_ns == 0);
            DWORD available = 1;
            check(PeekNamedPipe(read_handle, nullptr, 0, nullptr, &available, nullptr) && available == 0);
            first.clear_pending(); first.begin_command();
            check(a.commit() != SessionError::none);
        }
        // Coordinator destruction must not close the CLI-owned endpoint.
        const bool written = channel.inspect();
        check(written);
        if(written) {
            char data[8]{}; DWORD count = 0;
            check(ReadFile(read_handle, data, sizeof(data), &count, nullptr)
                && std::string_view(data, count) == "inspect\n");
        }
    }
    char data[8]{}; DWORD count = 0;
    check(!ReadFile(read_handle, data, sizeof(data), &count, nullptr) && GetLastError() == ERROR_BROKEN_PIPE);
    CloseHandle(read_handle);
    std::cout << checks << " analog coordinator checks, failures=" << failures << '\n';
    return failures ? 1 : 0;
}
