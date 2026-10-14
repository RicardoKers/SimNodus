// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "adapters/gdb/fixture_registers.hpp"
#include <iostream>
#include <string>
int main() {
    using namespace simnodus;
    const std::string raw = R"(93^done,register-values=[{number="0",value="0xdd5"},{number="13",value="0x20004fd8"},{number="14",value="0x80001b5"}])";
    std::array<std::uint32_t, 3> values{};
    unsigned cases = 0, failures = 0;
    auto check = [&](bool condition) { ++cases; if(!condition) ++failures; };
    check(gdb::parse_fixture_registers(raw,values) && values == std::array<std::uint32_t,3>{3541,0x20004fd8,0x80001b5});
    const auto original = values;
    for(std::size_t i = 0; i < raw.size(); ++i)
        check(!gdb::parse_fixture_registers(std::string_view(raw).substr(0,i),values) && values==original);
    for(const auto& text : {"", "-1", "+1", "100000000", "x", " 1"}) {
        auto bad = raw; bad.replace(bad.find("dd5"),3,text);
        check(!gdb::parse_fixture_registers(bad,values) && values==original);
    }
    auto max = raw; max.replace(max.find("dd5"),3,"ffffffff");
    check(gdb::parse_fixture_registers(max,values) && values[0]==0xffffffffU);
    check(!gdb::parse_fixture_registers(raw+raw,values));
    check(!gdb::parse_fixture_registers("0"+raw,values));
    auto correlated = std::string("3000010") + raw.substr(2);
    check(gdb::parse_fixture_registers(correlated,values,3000010));
    check(!gdb::parse_fixture_registers(correlated,values,4000010));
    check(!gdb::parse_fixture_registers(correlated,values));
    check(!gdb::parse_fixture_registers("0"+correlated,values,3000010));
    check(!gdb::parse_fixture_registers("30000100"+raw.substr(2),values,3000010));
    std::cout << cases << " register cases, failures=" << failures << '\n';
    return failures ? 1 : 0;
}
