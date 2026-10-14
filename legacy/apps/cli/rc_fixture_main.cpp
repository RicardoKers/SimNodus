// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "adapters/ngspice/rc_fixture.hpp"
int wmain(int argc, wchar_t** argv)
{
    return simnodus::ngspice::run_rc_fixture(argc, argv);
}
