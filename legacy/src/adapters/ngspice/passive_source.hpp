// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#pragma once
#include <array>
#include <memory>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace simnodus::spice {
inline constexpr std::size_t passive_source_max_bytes = 65536;
enum class PassiveKind { resistor, capacitor };
struct PassiveSubcircuit {
    std::string name;
    std::array<std::string, 2> terminals;
    std::string parameter, default_literal;
    PassiveKind kind;
    std::size_t begin, end;
};
struct PassiveSource { std::string bytes; std::vector<PassiveSubcircuit> subcircuits; };
struct PassiveSourceError { const char* code; std::size_t offset; };
using PassiveSourceResult = std::variant<std::shared_ptr<const PassiveSource>, PassiveSourceError>;
// Inspect caller-supplied bytes only. No engine/file I/O, physical verification,
// descriptor binding, numerical range certification or execution authority.
PassiveSourceResult inspect_passive_source(std::string_view bytes);
}
