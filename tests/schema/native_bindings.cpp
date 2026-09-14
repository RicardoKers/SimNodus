// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "application/binding_validation.hpp"
#include <iostream>
#include <type_traits>

int main()
{
    using namespace simnodus;
    static_assert(std::is_const_v<std::remove_reference_t<decltype(*std::get<0>(BindingResult{}))>>);
    std::string raw = R"({"format":"simnodus-topology","version":"0.3","root":"main","components":[{"id":"leaf","name":"Leaf","pins":[{"id":"p","name":"P","domain":"electrical"}],"parameters":[],"symbol":null,"model":{"definition":"interface","pin_map":{"p":"terminal"},"parameter_map":{}}}],"circuits":[{"id":"main","name":"Main","ports":[],"parameters":[],"nets":[],"instances":[{"id":"child","name":"Child","kind":"component","definition":"leaf","overrides":{}}],"symbol":null}],"symbols":[],"models":[{"id":"interface","name":"Interface","terminals":[{"id":"terminal","name":"Terminal","domain":"electrical"}],"parameters":[]}]})";
    const auto original = raw;
    auto result = validate_binding_declaration(raw);
    if(!std::holds_alternative<std::shared_ptr<const BindingDeclaration>>(result)) return 1;
    const auto declaration = std::get<0>(result);
    raw.assign("changed caller bytes");
    result = ParameterError{"input", 0};
    if(declaration->simulation_ready || declaration->descriptor_entries != 4
        || declaration->symbol_bindings != 0 || declaration->model_bindings != 1
        || declaration->parameters.instances.size() != 1) return 1;
    const auto& syntax = *declaration->parameters.topology.syntax;
    if(syntax.bytes != original) return 1;
    bool found_null = false, found_terminal = false;
    for(const auto& token : syntax.tokens) {
        if(token.kind == JsonKind::null) found_null = true;
        if(token.kind == JsonKind::string && token.decoded == "terminal"
            && token.begin == original.find("\"terminal\"")) found_terminal = true;
    }
    if(!found_null || !found_terminal) return 1;
    if(!std::holds_alternative<TopologyError>(validate_topology_declaration(original))
        || !std::holds_alternative<ParameterError>(resolve_parameter_declaration(original))) return 1;
    auto bad = original;
    auto position = bad.find("\"definition\":\"interface\"") + std::string("\"definition\":").size();
    bad.replace(position, std::string("\"interface\"").size(), "\"absent\"");
    const auto failure = validate_binding_declaration(bad);
    const auto* error = std::get_if<ParameterError>(&failure);
    if(!error || std::string_view(error->code) != "reference" || error->offset != position) return 1;
    std::cout << "Binding ownership, original map/null source tokens, readiness and version isolation passed\n";
}
