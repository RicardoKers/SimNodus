// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#pragma once
namespace simnodus::ngspice {
// Trusted explicit runtime paths only; this is not a circuit-file loader.
int run_rc_fixture(int argc, wchar_t** argv);
}
