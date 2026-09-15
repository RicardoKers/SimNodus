// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "application/project_graph.hpp"
#include <fstream>
#include <iostream>
#include <iterator>
#include <type_traits>

int main(int argc, char** argv)
{
    using namespace simnodus;
    static_assert(std::is_const_v<std::remove_reference_t<decltype(*std::get<0>(ProjectGraphResult{}))>>);
    if(argc != 2) return 1;
    std::ifstream input(argv[1], std::ios::binary);
    if(!input) return 1;
    std::string raw(std::istreambuf_iterator<char>{input}, {});
    const auto original = raw;
    auto result = load_project_graph(raw);
    if(!std::holds_alternative<std::shared_ptr<const ProjectGraph>>(result)) return 1;
    auto loaded = std::get<0>(result);
    result = LockError{"input", 0};
    raw = "caller changed";
    const auto& graph = loaded->connectivity;
    if(graph.root != "main" || graph.components.size() != 2 || graph.circuits.size() != 2) return 1;
    const auto declaration = loaded->declaration;
    if(declaration->sources.lock.syntax->bytes != original || declaration->sources.lock.simulation_ready
        || declaration->firmware_verified || declaration->runtime_profile_verified) return 1;
    const auto& instances = graph.circuits[0].instances;
    if(instances.size() != 2 || instances[0].identity.id != "left" || instances[1].identity.id != "right"
        || instances[0].definition != instances[1].definition || instances[0].kind != InstanceKind::circuit) return 1;
    const auto left = loaded->source_map.at({"circuits", "main", "instances", "left"});
    const auto right = loaded->source_map.at({"circuits", "main", "instances", "right"});
    if(left.begin == right.begin || original[left.begin] != '{' || original[right.begin] != '{') return 1;
    auto independent = graph;
    loaded.reset();
    independent.circuits[0].instances[0].identity.name = "Copy edit";
    independent.circuits[0].nets.clear();
    if(independent.circuits[0].instances[1].identity.name == "Copy edit"
        || declaration->sources.lock.syntax->bytes != original || independent.circuits[1].nets.empty()) return 1;
    auto invalid = original;
    const auto path = declaration->sources.lock.requests[0].path;
    const auto position = invalid.find('"' + path + '"');
    invalid.replace(position, path.size() + 2, "\"../bad\"");
    const auto bad = load_project_graph(invalid);
    const auto* error = std::get_if<LockError>(&bad);
    if(!error || std::string_view(error->code) != "path" || error->offset != position) return 1;
    std::cout << "Graph ownership, independent domain copy, scoped instance identity and full-project rejection passed\n";
}
