// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "application/project_acquisition.hpp"
#include "platform/project_acquisition_backend.hpp"
#include <iostream>
#include <stdexcept>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif
namespace {
std::string phase;
std::string field(std::size_t maximum)
{
    std::uint32_t size = 0;
    for(unsigned i = 0; i < 4; ++i) {
        const auto c = std::cin.get();
        if(c == std::char_traits<char>::eof()) throw std::runtime_error("truncated");
        size |= static_cast<std::uint32_t>(static_cast<unsigned char>(c)) << (8 * i);
    }
    if(size > maximum) throw std::runtime_error("budget");
    std::string text(size, '\0');
    std::cin.read(text.data(), static_cast<std::streamsize>(size));
    if(!std::cin) throw std::runtime_error("truncated");
    return text;
}
}
#ifdef SIMNODUS_ACQUISITION_TEST_HOOKS
void simnodus::acquisition_platform::test_boundary(const char* current)
{
    if(phase != current) return;
    std::cout << "PAUSE " << current << '\n' << std::flush;
    if(std::cin.get() != '!') throw std::runtime_error("resume");
}
#endif
int main(int argc, char** argv)
{
#ifdef _WIN32
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
#endif
    try {
#ifdef SIMNODUS_ACQUISITION_TEST_HOOKS
        if(argc == 2) phase = argv[1];
        else if(argc != 1) throw std::runtime_error("arguments");
        if(!phase.empty() && phase != "root" && phase != "opened" && phase != "captured" && phase != "released")
            throw std::runtime_error("phase");
#else
        (void)argv;
        if(argc != 1) throw std::runtime_error("arguments");
#endif
        const auto root = field(4097), name = field(1024);
        if(phase.empty() && std::cin.peek() != std::char_traits<char>::eof()) throw std::runtime_error("trailing");
        auto result = simnodus::acquire_project(root, name);
        if(const auto* error = std::get_if<simnodus::ProjectAcquisitionError>(&result)) {
            std::cout << "{\"stage\":\"" << (error->stage == simnodus::ProjectAcquisitionStage::physical ? "physical" : "declaration")
                << "\",\"error\":\"" << error->code << "\",\"offset\":" << error->offset << ",\"system\":" << error->system_code << "}\n";
            return 1;
        }
        const auto owned = std::get<0>(result);
        result = simnodus::ProjectAcquisitionError{simnodus::ProjectAcquisitionStage::physical, "released-test-result"};
        const auto& bytes = owned->declaration->sources.lock.syntax->bytes;
        std::cout << "{\"source_hex\":\"";
        for(unsigned char c : bytes) std::cout << "0123456789abcdef"[c >> 4] << "0123456789abcdef"[c & 15];
        std::cout << "\",\"components\":" << owned->connectivity.components.size()
            << ",\"circuits\":" << owned->connectivity.circuits.size()
            << ",\"spans\":" << owned->source_map.size() << "}\n";
    } catch(const std::exception&) { std::cout << "{\"error\":\"transport\"}\n"; return 1; }
}
