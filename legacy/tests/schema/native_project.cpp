// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "application/project_validation.hpp"
#include <fstream>
#include <iostream>
#include <iterator>
#include <type_traits>

int main(int argc, char** argv)
{
    using namespace simnodus;
    static_assert(std::is_const_v<std::remove_reference_t<decltype(*std::get<0>(ProjectResult{}))>>);
    if(argc != 2) return 1;
    // Only the test host reads its explicit owned fixture. The API takes bytes.
    std::ifstream file(argv[1], std::ios::binary);
    if(!file) return 1;
    std::string raw(std::istreambuf_iterator<char>{file}, {});
    const auto original = raw;
    auto result = validate_project_declaration(raw);
    if(!std::holds_alternative<std::shared_ptr<const ProjectDeclaration>>(result)) return 1;
    const auto project = std::get<0>(result);
    result = LockError{"input", 0};
    raw = "caller changed";
    const auto& links = project->sources;
    const auto& p = links.topology.parameters;
    if(project->runtime_profile_verified || project->firmware_verified || project->targets || project->project_entries
        || project->project_id != "two-rc-project" || links.resource_interfaces_verified || links.lock.simulation_ready
        || p.topology.syntax != links.lock.syntax || p.topology.syntax->bytes != original || p.instances.size() != 6) return 1;
    const auto& syntax = *links.lock.syntax;
    std::size_t sources_token = 0;
    for(std::size_t key = 1; key < syntax.tokens[0].next; key = syntax.tokens[key + 1].next)
        if(syntax.tokens[key].decoded == "sources") sources_token = key + 1;
    if(!sources_token) return 1;
    const auto& token = syntax.tokens[sources_token];
    const auto standalone = validate_resource_links(std::string_view(original).substr(token.begin, token.end - token.begin));
    if(!std::holds_alternative<std::shared_ptr<const ResourceLinkDeclaration>>(standalone)) return 1;
    const auto& expected = std::get<0>(standalone)->topology.parameters.instances;
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
    const auto& old_lock = std::get<0>(standalone)->lock;
    for(std::size_t i = 0; i < old_lock.requests.size(); ++i)
        if(links.lock.source_offsets[i] != old_lock.source_offsets[i] + token.begin) return 1;
    auto invalid = original;
    const auto source_path = links.lock.requests[0].path;
    const auto position = invalid.find('"' + source_path + '"');
    invalid.replace(position, source_path.size() + 2, "\"../bad\"");
    const auto bad = validate_project_declaration(invalid);
    const auto* error = std::get_if<LockError>(&bad);
    if(!error || std::string_view(error->code) != "path" || error->offset != position) return 1;
    if(!std::holds_alternative<LockError>(validate_resource_links(original))
        || !std::holds_alternative<LockError>(validate_lock_declaration(original))
        || !std::holds_alternative<ParameterError>(validate_binding_declaration(original))) return 1;
    std::cout << "Project immutable shared ownership, nested offsets, occurrence values and standalone version gates passed\n";
}
