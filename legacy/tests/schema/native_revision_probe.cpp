// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "application/project_revision.hpp"
#include "graph_probe_output.hpp"
#include <stdexcept>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif
namespace {
std::string field(std::size_t limit)
{
    std::uint32_t size = 0;
    for(unsigned i = 0; i < 4; ++i) {
        const auto c = std::cin.get();
        if(c == std::char_traits<char>::eof()) throw std::runtime_error("truncated");
        size |= static_cast<std::uint32_t>(static_cast<unsigned char>(c)) << (i * 8);
    }
    if(size > limit) throw std::runtime_error("limit");
    std::string bytes(size, '\0');
    std::cin.read(bytes.data(), static_cast<std::streamsize>(size));
    if(!std::cin) throw std::runtime_error("truncated");
    return bytes;
}
}
int main(int argc, char** argv)
{
#ifdef _WIN32
    _setmode(_fileno(stdin), _O_BINARY);
#endif
    try {
        const bool instance = argc == 2 && std::string_view(argv[1]) == "--instance";
        const bool resistance = argc == 2 && std::string_view(argv[1]) == "--resistance";
        const bool capacitance = argc == 2 && std::string_view(argv[1]) == "--capacitance";
        if(argc != 1 && !instance && !resistance && !capacitance) throw std::runtime_error("arguments");
        auto original = field(simnodus::declaration_max_bytes + 1);
        std::string circuit_id, instance_id;
        if(instance || resistance || capacitance) { circuit_id = field(1024); instance_id = field(1024); }
        auto name = field(1024);
        if(std::cin.peek() != std::char_traits<char>::eof()) throw std::runtime_error("trailing");
        auto result = capacitance ? simnodus::revise_capacitance(original, circuit_id, instance_id, name)
            : resistance ? simnodus::revise_resistance(original, circuit_id, instance_id, name)
            : instance ? simnodus::rename_instance(original, circuit_id, instance_id, name)
            : simnodus::rename_project(original, name);
        original.assign("released original input"); name.clear();
        if(const auto* error = std::get_if<simnodus::ProjectRevisionError>(&result)) {
            std::cout << "{\"error\":\"" << error->code << "\",\"stage\":\""
                << (error->stage == simnodus::ProjectRevisionStage::base ? "base" : "revision")
                << "\",\"offset\":" << error->offset << "}\n";
            return 1;
        }
        const auto graph = std::get<0>(result);
        result = simnodus::ProjectRevisionError{simnodus::ProjectRevisionStage::base, "released result", 0};
        std::cout << '{';
        graph_probe::write(*graph);
        if(resistance || capacitance) {
            const auto view = capacitance ? simnodus::literal_capacitance(*graph, circuit_id, instance_id)
                : simnodus::literal_resistance(*graph, circuit_id, instance_id);
            if(!view) throw std::runtime_error("missing view");
            std::cout << "\"literal\":{\"value\":"; graph_probe::quote(std::string(view->value));
            std::cout << ",\"unit\":"; graph_probe::quote(std::string(view->unit));
            std::cout << ",\"minimum\":"; graph_probe::quote(std::string(view->minimum));
            std::cout << ",\"maximum\":"; graph_probe::quote(std::string(view->maximum));
            std::cout << ",\"base_unit\":"; graph_probe::quote(std::string(view->base_unit));
            std::cout << "},\"parameters\":";
            graph_probe::array(graph->declaration->sources.topology.parameters.instances, [](const auto& row) {
                std::cout << "{\"path\":"; graph_probe::array(row.path, graph_probe::quote);
                std::cout << ",\"definition\":"; graph_probe::quote(row.definition);
                std::cout << ",\"parameters\":{";
                bool first = true;
                for(const auto& [id, value] : row.parameters) {
                    if(!first) std::cout << ','; first = false;
                    graph_probe::quote(id); std::cout << ":{\"value\":"; graph_probe::quote(value.value);
                    std::cout << ",\"unit\":"; graph_probe::quote(value.unit);
                    std::cout << ",\"origin\":"; graph_probe::quote(value.origin); std::cout << '}';
                }
                std::cout << "}}";
            });
            std::cout << ',';
        }
        circuit_id.clear(); instance_id.clear();
        std::cout << "\"source_hex\":\"";
        for(unsigned char c : graph->declaration->sources.lock.syntax->bytes)
            std::cout << "0123456789abcdef"[c >> 4] << "0123456789abcdef"[c & 15];
        std::cout << "\"}\n";
    } catch(const std::exception&) { std::cout << "{\"error\":\"transport\"}\n"; return 1; }
}
