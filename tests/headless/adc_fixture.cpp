// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "core/adc_fixture.hpp"
#include <limits>
#include <initializer_list>
int main()
{
    simnodus::AdcFixtureInput result;
    struct Case { double volts; unsigned uv, code; };
    for(const auto& c : {Case{0,0,0}, Case{0.00000049,0,0}, Case{0.0000005,1,0},
        Case{1.65,1650000,2048}, Case{2.853114,2853114,3541}, Case{3.3,3300000,4095}})
        if(!simnodus::AdcFixtureInput::prepare(c.volts,result) || result.microvolts!=c.uv || result.expected_code!=c.code) return 1;
    for(double value : {-0.1, 3.300001, std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()})
        if(simnodus::AdcFixtureInput::prepare(value,result)) return 1;
    return 0;
}
