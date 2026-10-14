// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// The process-wide ngspice session shared by all engine test cases
// (ngspice keeps global state, so there is only one per process).
#pragma once

#include "sim/ngspice_runtime.hpp"

namespace simnodus::test {

sim::NgspiceSession* engine();

} // namespace simnodus::test
