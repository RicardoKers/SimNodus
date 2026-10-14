// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>

namespace simnodus::sim {

struct ProcessMemory {
    std::uint64_t resident_bytes = 0; // working set (Windows) / RSS (Linux)
    std::uint64_t private_bytes = 0;  // commit charge (Windows) / data segment (Linux)
};

// Current memory use of this process; zeros if the platform query fails.
ProcessMemory current_process_memory();

} // namespace simnodus::sim
