// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "process_memory.hpp"

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#else
#include <fstream>
#include <unistd.h>
#endif

namespace simnodus::sim {

ProcessMemory current_process_memory()
{
    ProcessMemory result;
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS_EX counters{};
    counters.cb = sizeof(counters);
    if (K32GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters),
            sizeof(counters))) {
        result.resident_bytes = counters.WorkingSetSize;
        result.private_bytes = counters.PrivateUsage;
    }
#else
    // statm: size resident shared text lib data dt, in pages.
    std::ifstream statm("/proc/self/statm");
    std::uint64_t size = 0, resident = 0, shared = 0, text = 0, lib = 0, data = 0;
    if (statm >> size >> resident >> shared >> text >> lib >> data) {
        const auto page = static_cast<std::uint64_t>(sysconf(_SC_PAGESIZE));
        result.resident_bytes = resident * page;
        result.private_bytes = data * page;
    }
#endif
    return result;
}

} // namespace simnodus::sim
