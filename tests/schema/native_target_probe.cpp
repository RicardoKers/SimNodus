// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "application/reference_target.hpp"
#include <iostream>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif
void hex(const std::string& value)
{
    for(unsigned char c : value) std::cout << "0123456789abcdef"[c >> 4] << "0123456789abcdef"[c & 15];
    std::cout << '\n';
}
int main(int argc, char** argv)
{
#ifdef _WIN32
    _setmode(_fileno(stdin), _O_BINARY);
#endif
    if(argc != 2) return 2;
    std::string inputs[3];
    for(auto& input : inputs) {
        unsigned size = 0;
        for(unsigned i = 0; i < 4; ++i) {
            const auto c = std::cin.get();if(c < 0) return 2;
            size |= static_cast<unsigned>(c) << (8 * i);
        }
        if(size > simnodus::declaration_max_bytes) return 2;
        input.resize(size);std::cin.read(input.data(), size);if(!std::cin) return 2;
    }
    std::vector<std::string> path;
    std::size_t begin = 0;
    while(true) {
        const auto end = inputs[2].find('/', begin);
        path.push_back(inputs[2].substr(begin, end == std::string::npos ? end : end - begin));
        if(end == std::string::npos) break;
        begin = end + 1;
    }
    auto result = simnodus::inspect_reference_target(inputs[0], inputs[1], path,
        std::string_view(argv[1]) == "sn012" ? simnodus::firmware::BootProfile::sn012_stm32f103c8 : simnodus::firmware::BootProfile::unspecified);
    for(auto& input : inputs) input.assign("released");
    if(const auto* e = std::get_if<simnodus::ReferenceTargetError>(&result)) {
        std::cout << "ERR " << e->stage << ' ' << e->code << ' ' << e->offset << ' ' << e->coordinate << '\n';return 1;
    }
    const auto owned = std::get<0>(result);
    result = simnodus::ReferenceTargetError{"operation", "released"};
    // Test-only barrier; the caller may replace its fixture after capture.
    std::cout << "CAPTURED\n" << std::flush;
    if(std::cin.get() != 'c') return 2;
    const auto& r = *owned;const auto& f = *r.firmware;
    std::cout << "OK\n" << r.target_offset << ' ' << r.platform_offset << ' ' << r.platform_resource_index << '\n';
    std::cout << f.declaration->firmware_verified << ' ' << f.declaration->runtime_profile_verified << ' '
        << f.declaration->sources.topology.simulation_ready << '\n';
    hex(f.declaration->sources.lock.syntax->bytes);hex(f.elf->bytes);
    const auto& data = (*f.resources)[r.platform_resource_index].data;
    hex(std::string(reinterpret_cast<const char*>(data.data()), data.size()));
    std::cout << r.boot->stack << ' ' << r.boot->reset_vector << ' ' << r.boot->elf->load_ordered << '\n';
    for(const auto& [pin, destination] : r.pin_map) std::cout << pin << ' ' << destination << '\n';
}
