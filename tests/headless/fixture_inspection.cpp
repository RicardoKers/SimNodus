// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "application/fixture_inspection.hpp"
#include <iostream>
#include <sstream>

int main()
{
    using namespace simnodus;
    unsigned checks = 0, failures = 0;
    auto check = [&](bool value) { ++checks; if(!value) ++failures; };
    AdcFixtureInput adc;
    adc.expected_code = 3541;
    auto issue = [&](FixtureInspection& c, JointSession& s, std::string_view verb, bool activate = true) {
        std::istringstream input;
        return c.command(verb, input, s, activate, true, false, 1, adc);
    };
    FixtureInspection first, second;
    JointSession a(true), b(true);
    std::istringstream unrelated("5000000 0");
    check(!first.command("begin", unrelated, a, true, false, false, 0, adc));
    std::string remaining;
    std::getline(unrelated, remaining);
    check(remaining == "5000000 0" && a.snapshot().error == SessionError::none);
    check(issue(first, a, "enable-readback"));
    check(issue(second, b, "enable-inspection-time"));
    auto advance = [&](JointSession& s) {
        constexpr Nanoseconds boundary = 4027000;
        check(s.begin(5000000, false) == SessionError::none);
        check(s.observe(boundary) == SessionError::none);
        check(s.acknowledge({0, boundary, 5000000, 973000, true, {&boundary, 1}}) == SessionError::none);
        check(s.analog({boundary, boundary * 1e-9, 0}) == SessionError::none);
    };
    advance(a); advance(b);
    check(!first.commit_ready(a.snapshot()) && !second.commit_ready(b.snapshot()));
    const std::array<Nanoseconds, 9> words{4027000, 0x534E3035, 4, 1, 1, 3541, 1, 0, 0};
    first.normalized(words, a, true, false, 1, adc);
    check(a.snapshot().error == SessionError::none && a.snapshot().commits == 0);
    check(first.commit_ready(a.snapshot()) && !second.commit_ready(b.snapshot()));
    check(issue(second, b, "arm-readback"));
    check(!second.permits("commit") && second.permits("readback-timed") && second.permits("abort"));
    check(first.permits("commit"));
    second.normalized(words, b, true, false, 1, adc);
    check(b.snapshot().phase == SessionPhase::failed && !second.commit_ready(b.snapshot()));
    second.clear_pending();
    check(second.permits("quit") && b.commit() != SessionError::none && b.snapshot().commits == 0);
    check(first.commit_ready(a.snapshot()) && a.commit() == SessionError::none);
    FixtureInspection denied;
    JointSession denied_session(true);
    check(issue(denied, denied_session, "enable-readback", false));
    check(denied_session.snapshot().phase == SessionPhase::failed);
    std::cout << checks << " coordinator checks, failures=" << failures << '\n';
    return failures ? 1 : 0;
}
