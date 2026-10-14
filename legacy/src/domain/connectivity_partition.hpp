// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#pragma once
#include "domain/circuit_graph.hpp"
#include <compare>

namespace simnodus {
using OccurrencePath = std::vector<std::string>;
struct CompiledOccurrence { OccurrencePath path; std::string definition; InstanceKind kind; };
struct CompiledTerminal {
    OccurrencePath path;
    std::string terminal;
    auto operator<=>(const CompiledTerminal&) const = default;
};
struct CompiledNet {
    OccurrencePath path;
    std::string net;
    auto operator<=>(const CompiledNet&) const = default;
};
// Logical connectivity only: singleton terminals remain explicit; no ground,
// solver node, model, numerical value or runtime capability is assigned here.
struct ConnectivityGroup {
    std::vector<CompiledTerminal> terminals;
    std::vector<CompiledNet> source_nets;
};
}
