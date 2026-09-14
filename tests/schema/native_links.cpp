// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "application/resource_links.hpp"
#include <fstream>
#include <iostream>
#include <iterator>
#include <type_traits>

int main(int argc, char** argv)
{
    using namespace simnodus;
    static_assert(std::is_const_v<std::remove_reference_t<decltype(*std::get<0>(ResourceLinkResult{}))>>);
    if(argc != 2) return 1;
    // Explicit trusted fixture read in the test host, never in the validator.
    std::ifstream file(argv[1], std::ios::binary);
    if(!file) return 1;
    std::string raw(std::istreambuf_iterator<char>{file}, {});
    const auto original = raw;
    auto result = validate_resource_links(raw);
    if(!std::holds_alternative<std::shared_ptr<const ResourceLinkDeclaration>>(result)) return 1;
    const auto links = std::get<0>(result);
    result = LockError{"input", 0};
    raw = "caller changed";
    const auto& p = links->topology.parameters;
    if(links->resource_interfaces_verified || links->lock.simulation_ready || p.topology.syntax != links->lock.syntax
        || p.topology.syntax->bytes != original || links->descriptors_linked != 5 || links->assets != 2
        || p.instances.size() != 6 || links->lock.requests.size() != 3) return 1;
    for(std::size_t i = 0; i < links->lock.requests.size(); ++i) {
        if(original[links->lock.source_offsets[i]] != '{') return 1;
    }
    auto invalid = original;
    const auto source_path = links->lock.requests[0].path;
    const auto position = invalid.find('"' + source_path + '"');
    invalid.replace(position, source_path.size() + 2, "\"../bad\"");
    const auto bad = validate_resource_links(invalid);
    const auto* error = std::get_if<LockError>(&bad);
    if(!error || std::string_view(error->code) != "path" || error->offset != position) return 1;
    for(const auto& row : p.instances)
        for(const auto& [id, value] : row.parameters) {
            (void)id;
            if(original[value.source_offset] != '"') return 1;
        }
    const auto& syntax = *links->lock.syntax;
    std::size_t topology_token = 0;
    for(std::size_t key = 1; key < syntax.tokens[0].next; key = syntax.tokens[key + 1].next)
        if(syntax.tokens[key].decoded == "topology") topology_token = key + 1;
    if(!topology_token) return 1;
    const auto& token = syntax.tokens[topology_token];
    const auto standalone = validate_binding_declaration(std::string_view(original).substr(token.begin, token.end - token.begin));
    if(!std::holds_alternative<std::shared_ptr<const BindingDeclaration>>(standalone)) return 1;
    const auto& expected = std::get<0>(standalone)->parameters.instances;
    if(expected.size() != p.instances.size()) return 1;
    for(std::size_t i = 0; i < expected.size(); ++i) {
        const auto& row = p.instances[i];
        if(row.path != expected[i].path || row.definition != expected[i].definition
            || row.parameters.size() != expected[i].parameters.size()
            || row.source_offset != expected[i].source_offset + token.begin) return 1;
        for(const auto& [id, value] : row.parameters) {
            const auto& before = expected[i].parameters.at(id);
            if(value.value != before.value || value.unit != before.unit || value.origin != before.origin
                || value.source_offset != before.source_offset + token.begin) return 1;
        }
    }
    if(!std::holds_alternative<ParameterError>(validate_binding_declaration(original))
        || !std::holds_alternative<LockError>(validate_lock_declaration(original))) return 1;
    std::cout << "Links shared capture ownership, nested absolute offsets, retained occurrence values and version isolation passed\n";
}
