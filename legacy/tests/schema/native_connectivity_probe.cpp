// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "application/connectivity_compilation.hpp"
#include "graph_probe_output.hpp"
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif
namespace {
void path(const simnodus::OccurrencePath& value)
{
    std::cout << '[';
    bool first = true;
    for(const auto& part : value) { if(!first) std::cout << ','; first = false; graph_probe::quote(part); }
    std::cout << ']';
}
void span(simnodus::GraphSourceSpan value)
{
    std::cout << ",\"begin\":" << value.begin << ",\"end\":" << value.end;
}
}
int main()
{
#ifdef _WIN32
    _setmode(_fileno(stdin), _O_BINARY);
#endif
    std::string raw(simnodus::declaration_max_bytes + 1, '\0');
    std::cin.read(raw.data(), static_cast<std::streamsize>(raw.size())); raw.resize(static_cast<std::size_t>(std::cin.gcount()));
    auto result = simnodus::compile_connectivity(raw);
    raw.assign("released input");
    if(const auto* error = std::get_if<simnodus::ConnectivityCompilationError>(&result)) {
        std::cout << "{\"error\":\"" << error->code << "\",\"stage\":\""
            << (error->stage == simnodus::ConnectivityCompilationStage::declaration ? "declaration" : "expansion")
            << "\",\"offset\":" << error->offset << "}\n";
        return 1;
    }
    const auto owned = std::get<0>(result);
    result = simnodus::ConnectivityCompilationError{simnodus::ConnectivityCompilationStage::declaration, "released", 0};
    std::cout << "{\"occurrences\":[";
    bool first = true;
    for(const auto& occurrence : owned->occurrences) {
        if(!first) std::cout << ','; first = false;
        std::cout << "{\"path\":"; path(occurrence.path);
        std::cout << ",\"definition\":"; graph_probe::quote(occurrence.definition);
        std::cout << ",\"kind\":\"" << (occurrence.kind == simnodus::InstanceKind::circuit ? "circuit" : "component") << '"';
        span(owned->occurrence_sources.at(occurrence.path)); std::cout << '}';
    }
    std::cout << "],\"groups\":["; first = true;
    for(const auto& group : owned->connections) {
        if(!first) std::cout << ','; first = false;
        std::cout << "{\"terminals\":[";
        bool inner = true;
        for(const auto& terminal : group.terminals) {
            if(!inner) std::cout << ','; inner = false;
            std::cout << "{\"path\":"; path(terminal.path);
            std::cout << ",\"terminal\":"; graph_probe::quote(terminal.terminal);
            span(owned->terminal_sources.at(terminal)); std::cout << '}';
        }
        std::cout << "],\"nets\":["; inner = true;
        for(const auto& net : group.source_nets) {
            if(!inner) std::cout << ','; inner = false;
            std::cout << "{\"path\":"; path(net.path);
            std::cout << ",\"net\":"; graph_probe::quote(net.net);
            span(owned->net_sources.at(net)); std::cout << '}';
        }
        std::cout << "]}";
    }
    std::cout << "],\"source_hex\":\"";
    for(unsigned char c : owned->source->declaration->sources.lock.syntax->bytes)
        std::cout << "0123456789abcdef"[c >> 4] << "0123456789abcdef"[c & 15];
    std::cout << "\"}\n";
}
