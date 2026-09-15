// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "application/project_save.hpp"
#include "platform/project_save_backend.hpp"
#include <iostream>
#include <sstream>
#include <stdexcept>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif
namespace {
std::string phase;
bool inject = false;
std::string field(std::istream& input, std::size_t maximum)
{
    std::uint32_t size = 0;
    for(unsigned i = 0; i < 4; ++i) {
        const auto c = input.get();
        if(c == std::char_traits<char>::eof()) throw std::runtime_error("truncated");
        size |= static_cast<std::uint32_t>(static_cast<unsigned char>(c)) << (8 * i);
    }
    if(size > maximum) throw std::runtime_error("budget");
    std::string text(size, '\0');
    input.read(text.data(), static_cast<std::streamsize>(size));
    if(!input) throw std::runtime_error("truncated");
    return text;
}
}
#ifdef SIMNODUS_SAVE_TEST_HOOKS
bool simnodus::save_platform::test_boundary(const char* current)
{
    if(phase != current) return true;
    if(inject) return false;
    std::cout << "PAUSE " << current << '\n' << std::flush;
    return std::cin.get() == '!';
}
#endif
int main(int argc, char** argv)
{
#ifdef _WIN32
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
#endif
    try {
#ifdef SIMNODUS_SAVE_TEST_HOOKS
        if(argc >= 2) phase = argv[1];
        if(argc == 3 && std::string_view(argv[2]) == "fail") inject = true;
        else if(argc > 2) throw std::runtime_error("arguments");
        if(!phase.empty() && phase != "root" && phase != "created" && phase != "chunk"
            && phase != "written" && phase != "flushed" && phase != "publish") throw std::runtime_error("phase");
#else
        (void)argv;
        if(argc != 1) throw std::runtime_error("arguments");
#endif
        std::istringstream packet(field(std::cin, 1024 * 1024 + 8192));
        if(field(packet, 8) != "SNODSAV1") throw std::runtime_error("version");
        const auto root = field(packet, 4096), name = field(packet, 1024);
        const auto bytes = field(packet, 1024 * 1024 + 1);
        if(packet.peek() != std::char_traits<char>::eof()) throw std::runtime_error("trailing");
        if(phase.empty() && std::cin.peek() != std::char_traits<char>::eof()) throw std::runtime_error("trailing");
        const auto result = simnodus::save_new_project(root, name, bytes);
        if(const auto* error = std::get_if<simnodus::ProjectSaveError>(&result)) {
            std::cout << "{\"error\":\"" << error->code << "\",\"offset\":" << error->offset
                << ",\"system\":" << error->system_code << ",\"cleanup_failed\":" << std::boolalpha << error->temporary_cleanup_failed << "}\n";
            return 1;
        }
        std::cout << "{\"saved\":true,\"bytes\":" << std::get<0>(result).bytes << "}\n";
    } catch(const std::exception&) {
        std::cout << "{\"error\":\"transport\",\"offset\":0,\"system\":0,\"cleanup_failed\":false}\n";
        return 1;
    }
}
