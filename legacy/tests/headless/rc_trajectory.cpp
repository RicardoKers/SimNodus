// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "core/rc_trajectory.hpp"
#include <limits>
#include <iostream>
int main() {
    using namespace simnodus;
    unsigned failures = 0;
    auto check = [&](bool value) { if(!value) ++failures; };
    check(RcTrajectory::accepts({0, 0, 0}, false));
    check(RcTrajectory::accepts({2'011'375, .002011375, 0}, false));
    check(RcTrajectory::accepts({2'011'375, .002011375, 0}, true));
    check(!RcTrajectory::accepts({2'011'374, .002011374, 0}, true));
    check(!RcTrajectory::accepts({2'011'376, .002011376, 0}, false));
    check(RcTrajectory::accepts({3'011'375, .003011375, 2.0859978441342406}, true));
    check(!RcTrajectory::accepts({3'011'375, .003011375, 2.08602}, true));
    check(!RcTrajectory::accepts({3'011'375, .003011375, 2.08597}, true));
    check(RcTrajectory::accepts({0, 0, 1e-5}, false));
    check(!RcTrajectory::accepts({0, 0, std::nextafter(1e-5, 1.)}, false));
    check(!RcTrajectory::accepts({0, 0, -std::nextafter(1e-5, 1.)}, false));
    check(!RcTrajectory::accepts({0, 2e-12, 0}, false));
    check(!RcTrajectory::accepts({0, 0, std::numeric_limits<double>::quiet_NaN()}, false));
    check(!RcTrajectory::accepts({0, std::numeric_limits<double>::infinity(), 0}, false));
    check(RcTrajectory::accepts({100'000'000, .1, 3.3}, true));
    std::cout << "15 trajectory cases, failures=" << failures << '\n';
    return failures ? 1 : 0;
}
