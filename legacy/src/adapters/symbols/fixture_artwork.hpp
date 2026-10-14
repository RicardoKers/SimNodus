// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#pragma once
#include <array>
#include <optional>
#include <string_view>

namespace simnodus::symbols {
struct ArtworkLine { double x1, y1, x2, y2; };
struct FixtureArtwork { double width, height; std::array<ArtworkLine, 6> lines; };
struct FixtureAnchor { double x, y; };
// Explicit private convention for the recognized original artwork only:
// two-pin/a = left lead endpoint, two-pin/b = right. No SVG-derived anchor ABI.
std::optional<FixtureAnchor> fixture_anchor(std::string_view symbol, std::string_view pin) noexcept;
// Closed recognizer for one original owned fixture. No SVG parser/anchor ABI.
// Coordinates own their values and require no retained input.
std::optional<FixtureArtwork> recognize_fixture_artwork(std::string_view bytes) noexcept;
}
