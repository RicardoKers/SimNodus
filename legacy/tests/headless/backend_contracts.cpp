// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "core/backend_contracts.hpp"
#include <iostream>
#include <limits>
int main()
{
    using namespace simnodus;
    int failures = 0;
    auto check = [&](bool passed) { if(!passed) ++failures; };
    check(RcFixtureContract::request(0, 2011375, 10000000) == BoundaryError::none);
    for(auto target : {Nanoseconds{0}, Nanoseconds{1}, Nanoseconds{10000000},
                       std::numeric_limits<Nanoseconds>::max()})
        check(RcFixtureContract::request(1, target, 10000000) == BoundaryError::invalid_request);
    check(RcFixtureContract::request(0, 1, 10000001) == BoundaryError::invalid_request);
    check(RcFixtureContract::observation({3140000, .00314, 2.0}) == BoundaryError::none);
    check(RcFixtureContract::observation({3140000, .003140000002, 2.0}) == BoundaryError::endpoint_mismatch);
    check(RcFixtureContract::observation({1, std::numeric_limits<double>::quiet_NaN(), 0}) == BoundaryError::nonfinite);
    check(RcFixtureContract::observation({1, 1e-9, std::numeric_limits<double>::infinity()}) == BoundaryError::nonfinite);
    std::cout << failures << " contract failures\n";
    return failures ? 1 : 0;
}
