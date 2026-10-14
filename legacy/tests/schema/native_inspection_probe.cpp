// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "application/project_inspection.hpp"
#include "graph_probe_output.hpp"
#include <algorithm>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

int main(int argc, char** argv)
{
    if(argc != 3) return 2;
#ifdef _WIN32
    _setmode(_fileno(stdin), _O_BINARY);
#endif
    std::string raw(simnodus::declaration_max_bytes + 1, '\0');
    std::cin.read(raw.data(), static_cast<std::streamsize>(raw.size()));
    raw.resize(static_cast<std::size_t>(std::cin.gcount()));
    auto result = simnodus::load_project_graph(raw);
    if(const auto* error = std::get_if<simnodus::LockError>(&result)) {
        std::cout << "{\"error\":\"" << error->code << "\",\"offset\":" << error->offset << "}\n";
        return 1;
    }
    auto graph = std::get<0>(result);
    result = simnodus::LockError{"released-test-owner", 0};
    const auto original = raw;
    raw.assign(raw.size(), 'x'); // Caller input and load result no longer own rows.
    const auto before = graph->source_map;
    const auto rows = simnodus::inspect_instance_parameters(*graph, argv[1], argv[2]);
    const auto& owned = graph->declaration->sources.topology.parameters.instances;
    const auto& retained_bytes = graph->declaration->sources.lock.syntax->bytes;
    bool borrowed = true;
    for(const auto* row : rows)
        borrowed = borrowed && std::any_of(owned.begin(), owned.end(), [row](const auto& item) { return &item == row; });
    bool spans_unchanged = before.size() == graph->source_map.size();
    for(const auto& [id, span] : before) {
        const auto after = graph->source_map.find(id);
        spans_unchanged = spans_unchanged && after != graph->source_map.end() &&
            span.begin == after->second.begin && span.end == after->second.end;
    }
    std::cout << std::boolalpha << "{\"borrowed_owned\":" << borrowed << ",\"nonmutating\":"
        << (spans_unchanged && retained_bytes == original) << ",\"rows\":";
    graph_probe::array(rows, [](const simnodus::ParameterOccurrence* row) {
        std::cout << "{\"path\":"; graph_probe::array(row->path, graph_probe::quote);
        std::cout << ",\"definition\":"; graph_probe::quote(row->definition);
        std::cout << ",\"source_offset\":" << row->source_offset << ",\"parameters\":{";
        bool first = true;
        for(const auto& [id, parameter] : row->parameters) {
            if(!first) std::cout << ',';
            first = false;
            graph_probe::quote(id); std::cout << ":{\"value\":"; graph_probe::quote(parameter.value);
            std::cout << ",\"unit\":"; graph_probe::quote(parameter.unit);
            std::cout << ",\"origin\":"; graph_probe::quote(parameter.origin);
            std::cout << ",\"source_offset\":" << parameter.source_offset << '}';
        }
        std::cout << "}}";
    });
    std::cout << "}\n";
}
