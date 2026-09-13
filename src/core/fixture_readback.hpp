// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#pragma once
#include "backend_contracts.hpp"
#include <array>
#include <span>
namespace simnodus {
struct FixtureReadback {
    static constexpr Nanoseconds boundary_ns = 4'027'000;
    static bool accepts(Nanoseconds time, std::span<const Nanoseconds> words, std::uint32_t code) noexcept
    {
        const std::array<Nanoseconds, 8> expected{0x534E3035, 4, 1, 1, code, 1, 0, 0};
        if(time != boundary_ns || code > 4095 || words.size() != expected.size()) return false;
        for(std::size_t i = 0; i < expected.size(); ++i) if(words[i] != expected[i]) return false;
        return true;
    }
};
} // namespace simnodus
