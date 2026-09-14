// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "application/topology_validation.hpp"
#include <iostream>
#include <type_traits>

int main()
{
    using namespace simnodus;
    static_assert(std::is_const_v<std::remove_reference_t<decltype(*TopologyDeclaration{}.syntax)>>);
    std::string raw = R"({"format":"simnodus-topology","version":"0.1","root":"main","components":[],"circuits":[{"id":"main","name":"Empty","ports":[],"instances":[],"nets":[]}]})";
    const auto saved = raw;
    auto result = validate_topology_declaration(raw);
    if(!std::holds_alternative<TopologyDeclaration>(result)) return 1;
    const auto topology = std::get<TopologyDeclaration>(result);
    raw.assign("caller changed");
    result = TopologyError{TopologyErrorCode::input, 0};
    if(topology.syntax->bytes != saved || topology.root != "main" || topology.declared_entities != 1
        || topology.root_depth != 1 || topology.root_expanded_instances != 0) return 1;
    auto bad = saved;
    const auto offset = bad.find("\"main\"");
    bad.replace(offset, 6, "\"missing\"");
    const auto rejected = validate_topology_declaration(bad);
    const auto* error = std::get_if<TopologyError>(&rejected);
    if(!error || error->code != TopologyErrorCode::reference || error->offset != offset) return 1;
    const auto malformed = validate_topology_declaration("{");
    if(!std::holds_alternative<TopologyError>(malformed)
        || std::get<TopologyError>(malformed).code != TopologyErrorCode::input) return 1;
    std::cout << "Topology ownership, root statistics, exact error offset and input rejection passed\n";
}
