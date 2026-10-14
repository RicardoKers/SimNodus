// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "adapters/symbols/fixture_artwork.hpp"

namespace simnodus::symbols {
std::optional<FixtureAnchor> fixture_anchor(std::string_view symbol, std::string_view pin) noexcept
{
    if(symbol != "two-pin") return {};
    if(pin == "a") return FixtureAnchor{0, 20};
    if(pin == "b") return FixtureAnchor{100, 20};
    return {};
}
std::optional<FixtureArtwork> recognize_fixture_artwork(std::string_view bytes) noexcept
{
    constexpr std::string_view original =
        "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 100 40\">\n"
        "  <!-- Original SimNodus schema fixture, MIT. Not a validated renderer asset. -->\n"
        "  <path d=\"M0 20H25M75 20H100M25 10H75V30H25Z\" fill=\"none\" stroke=\"black\"/>\n"
        "</svg>\n";
    if(bytes != original) return {};
    // Coordinates from the exact accepted path; no markup is executed.
    return FixtureArtwork{100, 40, {{{0, 20, 25, 20}, {75, 20, 100, 20},
        {25, 10, 75, 10}, {75, 10, 75, 30}, {75, 30, 25, 30}, {25, 30, 25, 10}}}};
}
}
