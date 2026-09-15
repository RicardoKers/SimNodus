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
int main()
{
#ifdef _WIN32
    _setmode(_fileno(stdin), _O_BINARY);
#endif
    try {
        auto original = field(simnodus::declaration_max_bytes + 1), name = field(1024);
        if(std::cin.peek() != std::char_traits<char>::eof()) throw std::runtime_error("trailing");
        auto result = simnodus::rename_project(original, name);
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
        std::cout << "\"source_hex\":\"";
        for(unsigned char c : graph->declaration->sources.lock.syntax->bytes)
            std::cout << "0123456789abcdef"[c >> 4] << "0123456789abcdef"[c & 15];
        std::cout << "\"}\n";
    } catch(const std::exception&) { std::cout << "{\"error\":\"transport\"}\n"; return 1; }
}
