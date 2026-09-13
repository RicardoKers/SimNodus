// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "core/fixture_readback.hpp"
#include <iostream>
int main() {
    using namespace simnodus;
    std::array<Nanoseconds, 8> words{0x534E3035, 4, 1, 1, 3541, 1, 0, 0};
    unsigned failed = 0;
    auto check = [&](bool condition) { if(!condition) ++failed; };
    check(FixtureReadback::accepts(4027000, words, 3541));
    for(auto& word : words) {
        ++word; check(!FixtureReadback::accepts(4027000, words, 3541)); --word;
    }
    check(!FixtureReadback::accepts(4026999, words, 3541));
    check(!FixtureReadback::accepts(4027001, words, 3541));
    check(!FixtureReadback::accepts(4027000, std::span<const Nanoseconds>(words).first(7), 3541));
    check(!FixtureReadback::accepts(4027000, words, 3540));
    words[4] = 0; check(FixtureReadback::accepts(4027000, words, 0));
    words[4] = 4095; check(FixtureReadback::accepts(4027000, words, 4095));
    words[4] = 4096; check(!FixtureReadback::accepts(4027000, words, 4096));
    std::cout << "16 readback cases, failures=" << failed << '\n';
    return failed ? 1 : 0;
}
