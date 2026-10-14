// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "application/fixture_adc.hpp"
#include <iostream>
#include <sstream>
#include <type_traits>
static_assert(!std::is_copy_constructible_v<simnodus::FixtureAdc>);
int main()
{
    using namespace simnodus;
    unsigned checks = 0, failures = 0;
    auto check = [&](bool ok) { ++checks; if(!ok) ++failures; };
    FixtureAdc first({}, 0), second({}, 0);
    JointSession a(true), b(true);
    constexpr Nanoseconds boundary = 4010750;
    const AnalogObservation sample{boundary, boundary * 1e-9, 2.853114};
    auto ready = [&](JointSession& s) {
        check(s.begin(5000000, false) == SessionError::none);
        check(s.observe(boundary) == SessionError::none);
        check(s.acknowledge({0, boundary, 5000000, 989250, true, {&boundary, 1}}) == SessionError::none);
        check(s.analog(sample) == SessionError::none);
    };
    ready(a); ready(b);
    std::istringstream unknown("retained argument");
    check(!first.command("inspect-native", unknown, a, true, sample));
    std::string remaining; std::getline(unknown, remaining);
    check(remaining == "retained argument");
    auto issue = [&](FixtureAdc& adc, JointSession& s, std::string_view verb, std::string args = "", bool allowed = true) {
        std::istringstream input(args);
        return adc.command(verb, input, s, allowed, sample);
    };
    check(issue(first, a, "prepare-adc"));
    check(first.prepared() && first.pending() && first.confirmations() == 0);
    check(!second.prepared() && !second.pending() && second.confirmations() == 0);
    check(!first.permits("commit") && first.permits("abort") && first.permits("adc-applied"));
    check(first.input().microvolts == 2853114 && first.input().expected_code == 3541);
    check(issue(first, a, "adc-applied", "0 2853114 4010 4010"));
    check(first.confirmations() == 1 && !first.pending() && a.snapshot().commits == 0);
    check(issue(first, a, "adc-applied", "0 2853114 4010 4010"));
    check(a.snapshot().phase == SessionPhase::failed && first.confirmations() == 1);
    check(issue(second, b, "prepare-adc", "", false));
    check(b.snapshot().phase == SessionPhase::failed && !second.prepared());
    first.cancel(); second.cancel();
    check(first.confirmations() == 1 && !first.pending());
    check(a.commit() != SessionError::none && a.snapshot().commits == 0);
    std::cout << checks << " ADC coordinator checks, failures=" << failures << '\n';
    return failures ? 1 : 0;
}
