// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "adapters/gdb/fixture_memory.hpp"
#include <iostream>
#include <string>
int main() {
    using namespace simnodus;
    const std::string raw = "91^done,memory=[{begin=\"0x20000000\",offset=\"0x00000000\",end=\"0x20000020\",contents=\"000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f\"}]";
    std::array<Nanoseconds, 8> words{};
    unsigned failures = 0, cases = 0;
    auto check = [&](bool condition) { ++cases; if(!condition) ++failures; };
    check(gdb::parse_fixture_memory(raw, words));
    check(words == std::array<Nanoseconds, 8>{0x03020100,0x07060504,0x0b0a0908,0x0f0e0d0c,
                                            0x13121110,0x17161514,0x1b1a1918,0x1f1e1d1c});
    const auto original = words;
    for(std::size_t i = 0; i < raw.size(); ++i) {
        check(!gdb::parse_fixture_memory(std::string_view(raw).substr(0, i), words) && words == original);
    }
    const auto content = raw.find("00010203");
    for(std::size_t i = 0; i < 64; ++i) {
        auto bad = raw; bad[content+i] = 'x';
        check(!gdb::parse_fixture_memory(bad, words) && words == original);
    }
    check(!gdb::parse_fixture_memory(raw + "\n", words) && words == original);
    check(!gdb::parse_fixture_memory(raw + raw, words) && words == original);
    auto correlated = std::string("1000010") + raw.substr(2);
    check(gdb::parse_fixture_memory(correlated, words, 1000010));
    check(!gdb::parse_fixture_memory(correlated, words, 1000011));
    check(!gdb::parse_fixture_memory(correlated, words));
    check(!gdb::parse_fixture_memory("0" + correlated, words, 1000010));
    check(!gdb::parse_fixture_memory("+" + correlated, words, 1000010));
    check(!gdb::parse_fixture_memory("10000100" + raw.substr(2), words, 1000010));
    std::cout << cases << " memory frame cases, failures=" << failures << '\n';
    return failures ? 1 : 0;
}
