// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "application/fixture_execution.hpp"
#include <windows.h>
#include <iostream>
#include <sstream>
#include <type_traits>
static_assert(!std::is_copy_constructible_v<simnodus::FixtureExecution>);
int main()
{
    using namespace simnodus;
    unsigned checks = 0, failures = 0;
    auto check = [&](bool ok) { ++checks; if(!ok) ++failures; };
    HANDLE read_handle = nullptr, write_handle = nullptr;
    if(!CreatePipe(&read_handle, &write_handle, nullptr, 0)) return 1;
    {
        renode::ControlChannel channel(reinterpret_cast<std::uintptr_t>(write_handle));
        check(channel.valid());
        {
            FixtureExecution first(&channel), second(nullptr);
            JointSession a(true), b(true);
            std::istringstream unrelated("unchanged arguments");
            check(!first.command("prepare-adc", unrelated, a));
            std::string text; std::getline(unrelated, text);
            check(text == "unchanged arguments" && a.snapshot().error == SessionError::none);
            first.begin(a, 5000000, false);
            check(a.snapshot().phase == SessionPhase::granted && a.snapshot().commits == 0);
            check(b.snapshot().phase == SessionPhase::stopped);
            check(first.commit_ready());
            std::istringstream empty;
            check(first.command("cancel-native", empty, a));
            check(a.snapshot().phase == SessionPhase::failed && a.snapshot().commits == 0);
            check(b.snapshot().phase == SessionPhase::stopped);
            DWORD available = 1;
            check(PeekNamedPipe(read_handle, nullptr, 0, nullptr, &available, nullptr) && available == 0);
            first.suppress_reply();
            check(a.commit() != SessionError::none);
        }
        // Coordinator destruction must not close the CLI-owned endpoint.
        const bool written = channel.cancel();
        check(written);
        if(written) {
            char data[64]{}; DWORD count = 0;
            check(ReadFile(read_handle, data, sizeof(data), &count, nullptr)
                && std::string_view(data, count) == "emulation CancelCancellationProbe\n");
        }
    }
    char data[64]{}; DWORD count = 0;
    check(!ReadFile(read_handle, data, sizeof(data), &count, nullptr) && GetLastError() == ERROR_BROKEN_PIPE);
    CloseHandle(read_handle);
    std::cout << checks << " execution coordinator checks, failures=" << failures << '\n';
    return failures ? 1 : 0;
}
