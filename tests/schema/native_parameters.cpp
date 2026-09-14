// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "application/parameter_validation.hpp"
#include <iostream>
#include <type_traits>

int main()
{
    using namespace simnodus;
    static_assert(std::is_const_v<std::remove_reference_t<decltype(*std::get<0>(ParameterResult{}))>>);
    std::string raw = R"({"format":"simnodus-topology","version":"0.2","root":"main","components":[{"id":"leaf","name":"Leaf","pins":[{"id":"p","name":"P","domain":"electrical"}],"parameters":[{"id":"p","name":"P","unit":"1","default":"-0.00","minimum":"-1","maximum":"1"}]}],"circuits":[{"id":"main","name":"Main","ports":[],"parameters":[],"nets":[],"instances":[{"id":"child","name":"Child","kind":"component","definition":"leaf","overrides":{}}]}]})";
    const auto original = raw;
    auto result = resolve_parameter_declaration(raw);
    if(!std::holds_alternative<std::shared_ptr<const ParameterSnapshot>>(result)) return 1;
    const auto snapshot = std::get<0>(result);
    raw.assign("changed caller bytes");
    result = ParameterError{"input", 0};
    if(snapshot->topology.syntax->bytes != original || snapshot->instances.size() != 1
        || snapshot->parameter_entries != 1 || snapshot->resolved_entries != 2) return 1;
    const auto& row = snapshot->instances[0];
    const auto& value = row.parameters.at("p");
    if(row.path != std::vector<std::string>{"main", "child"} || value.value != "-0.00"
        || value.unit != "1" || value.origin != "default" || value.source_offset != original.find("\"-0.00\"")
        || row.source_offset != original.find("{\"id\":\"child\"")) return 1;
    auto invalid = original;
    invalid.replace(invalid.find("-0.00"), 5, "1E0");
    const auto failure = resolve_parameter_declaration(invalid);
    const auto* error = std::get_if<ParameterError>(&failure);
    if(!error || std::string_view(error->code) != "quantity" || error->offset != invalid.find("\"1E0\"")) return 1;
    if(!std::holds_alternative<TopologyError>(validate_topology_declaration(original))) return 1;
    auto literal = original;
    auto position = literal.find("\"overrides\":{}");
    literal.replace(position, std::string("\"overrides\":{}").size(), R"("overrides":{"p":{"value":"1e-1","unit":"1"}})");
    const auto literal_result = resolve_parameter_declaration(literal);
    if(!std::holds_alternative<std::shared_ptr<const ParameterSnapshot>>(literal_result)) return 1;
    const auto& literal_value = std::get<0>(literal_result)->instances[0].parameters.at("p");
    if(literal_value.value != "0.1" || literal_value.origin != "literal" || literal_value.source_offset != literal.find("\"1e-1\"")) return 1;
    auto forwarded = original;
    position = forwarded.find("\"parameters\":[]");
    forwarded.replace(position, std::string("\"parameters\":[]").size(),
        R"("parameters":[{"id":"p","name":"Parent","unit":"1","default":"-0.500","minimum":"-1","maximum":"1"}])");
    position = forwarded.find("\"overrides\":{}");
    forwarded.replace(position, std::string("\"overrides\":{}").size(), R"("overrides":{"p":{"parameter":"p"}})");
    const auto forwarded_result = resolve_parameter_declaration(forwarded);
    if(!std::holds_alternative<std::shared_ptr<const ParameterSnapshot>>(forwarded_result)) return 1;
    const auto& forwarded_value = std::get<0>(forwarded_result)->instances[0].parameters.at("p");
    if(forwarded_value.value != "-0.500" || forwarded_value.origin != "containing-circuit:p"
        || forwarded_value.source_offset != forwarded.find("\"parameter\":\"p\"") + std::string("\"parameter\":").size()) return 1;
    std::cout << "Parameter ownership, independent namespaces, exact signed zero, source offsets and version isolation passed\n";
}
