// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "adapters/gdb/fixture_time.hpp"
#include <iostream>
#include <string>
int main() {
    using namespace simnodus;
    const std::string stream = R"(@"[Domain = Elapsed Virtual Time: 00:00:00.004027000\r\n")";
    const std::string done = "2000010^done";
    Nanoseconds output = 17;
    unsigned cases = 0, failed = 0;
    auto check = [&](bool condition) { ++cases; if(!condition) ++failed; };
    check(gdb::parse_fixture_time(stream, done, 2000010, output) && output == 4027000);
    for(std::size_t i = 0; i < stream.size(); ++i) {
        output = 17;
        check(!gdb::parse_fixture_time(std::string_view(stream).substr(0,i), done, 2000010, output) && output == 17);
    }
    for(const auto& completion : {"2000011^done", "02000010^done", "+2000010^done", "20000100^done", "2000010^error", "2000010^done extra", ""}) {
        output = 17;
        check(!gdb::parse_fixture_time(stream, completion, 2000010, output) && output == 17);
    }
    const auto start = stream.find("00:00:00");
    for(const auto& time : {"00:60:00.004027000", "00:00:60.004027000", "-1:00:00.004027000", "00:00:00.00402700x", "00:00:00.00402700"}) {
        auto changed = stream; changed.replace(start,18,time); output = 17;
        check(!gdb::parse_fixture_time(changed, done, 2000010, output) && output == 17);
    }
    auto largest = stream; largest.replace(start,18,"99:59:59.999999999");
    check(gdb::parse_fixture_time(largest,done,2000010,output) && output == 359999999999999ULL);
    check(!gdb::parse_fixture_time(stream+stream,done,2000010,output));
    std::cout << cases << " time frame cases, failures=" << failed << '\n';
    return failed ? 1 : 0;
}
