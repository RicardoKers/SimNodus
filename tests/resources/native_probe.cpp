// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
// Explicit test transport, not a project parser or execution entrypoint.
#include "application/local_resources.hpp"
#include "platform/local_resources_backend.hpp"
#include <iostream>
#include <sstream>
#include <stdexcept>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

namespace {
std::string pause_phase;
std::uint64_t number(std::istream& input, unsigned width)
{
    std::uint64_t value = 0;
    for(unsigned i = 0; i < width; ++i) {
        const auto c = input.get();
        if(c == std::char_traits<char>::eof()) throw std::runtime_error("truncated");
        value |= static_cast<std::uint64_t>(static_cast<unsigned char>(c)) << (8 * i);
    }
    return value;
}
std::string text(std::istream& input, std::size_t maximum)
{
    const auto length = number(input, 4);
    if(length > maximum) throw std::runtime_error("budget");
    std::string value(static_cast<std::size_t>(length), '\0');
    input.read(value.data(), static_cast<std::streamsize>(value.size()));
    if(!input) throw std::runtime_error("truncated");
    return value;
}
void write_number(std::uint64_t value, unsigned width)
{
    for(unsigned i = 0; i < width; ++i) std::cout.put(static_cast<char>((value >> (8 * i)) & 255));
}
void write_text(const std::string& value)
{
    write_number(value.size(), 4);
    std::cout.write(value.data(), static_cast<std::streamsize>(value.size()));
}
}

#ifdef SIMNODUS_RESOURCE_TEST_HOOKS
void simnodus::resource_platform::test_boundary(const char* phase, std::size_t index)
{
    if(pause_phase != phase || (index != resource_global_error && index != 0)) return;
    std::cout << "PAUSE " << phase << '\n' << std::flush;
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
#ifdef SIMNODUS_RESOURCE_TEST_HOOKS
        if(argc == 2) pause_phase = argv[1];
        if(argc > 2 || (!pause_phase.empty() && pause_phase != "root"
            && pause_phase != "opened" && pause_phase != "captured")) throw std::runtime_error("arguments");
#else
        (void)argv;
        if(argc != 1) throw std::runtime_error("arguments");
#endif
        const auto packet = text(std::cin, 1024 * 1024);
        if(pause_phase.empty() && std::cin.peek() != std::char_traits<char>::eof())
            throw std::runtime_error("trailing");
        std::istringstream input(packet);
        if(text(input, 16) != "SNODRES1") throw std::runtime_error("version");
        const auto root = text(input, 4096);
        const auto count = number(input, 4);
        if(count > simnodus::resource_max_files) throw std::runtime_error("count");
        std::vector<simnodus::ResourceRequest> requests;
        for(std::uint64_t i = 0; i < count; ++i) {
            simnodus::ResourceRequest r;
            r.dependency = text(input, 64);
            r.resource = text(input, 64);
            r.path = text(input, 240);
            r.bytes = number(input, 8);
            r.sha256 = text(input, 64);
            requests.push_back(std::move(r));
        }
        if(input.peek() != std::char_traits<char>::eof()) throw std::runtime_error("trailing");
        const auto result = simnodus::verify_local_resources(root, requests);
        if(const auto* error = std::get_if<simnodus::ResourceError>(&result)) {
            std::cout << "ERR " << simnodus::resource_error_name(error->code) << ' '
                << error->index << ' ' << error->system_code << '\n';
            return 1;
        }
        const auto& snapshots = *std::get<simnodus::ResourceSnapshots>(result);
        std::cout << "OK\n";
        write_number(snapshots.size(), 4);
        for(const auto& snapshot : snapshots) {
            write_text(snapshot.dependency);
            write_text(snapshot.resource);
            write_text(snapshot.path);
            write_text(snapshot.sha256);
            write_number(snapshot.data.size(), 8);
            std::cout.write(reinterpret_cast<const char*>(snapshot.data.data()),
                static_cast<std::streamsize>(snapshot.data.size()));
        }
        return 0;
    } catch(const std::exception&) {
        std::cout << "ERR input 0 0\n";
        return 1;
    }
}
