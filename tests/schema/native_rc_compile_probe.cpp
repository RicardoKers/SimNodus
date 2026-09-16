// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "application/ideal_rc_compilation.hpp"
#include <iostream>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif
void hex(const std::string& value)
{
    constexpr char digits[] = "0123456789abcdef";
    for(unsigned char c : value) std::cout << digits[c >> 4] << digits[c & 15];
    std::cout << '\n';
}
int main()
{
#ifdef _WIN32
    _setmode(_fileno(stdin), _O_BINARY);
#endif
    std::string inputs[6];
    for(auto& input : inputs) {
        unsigned size = 0;
        for(unsigned i = 0; i < 4; ++i) {
            const auto byte = std::cin.get(); if(byte < 0) return 2;
            size |= static_cast<unsigned>(byte) << (8 * i);
        }
        if(size > simnodus::declaration_max_bytes + 1) return 2;
        input.resize(size); std::cin.read(input.data(), size);
        if(!std::cin) return 2;
    }
    simnodus::IdealRcRequest request{inputs[2] == "e01" ? simnodus::IdealRcProfile::e01_ideal_rc
        : simnodus::IdealRcProfile::unspecified, inputs[3], inputs[4], inputs[5]};
    auto result = simnodus::compile_ideal_rc(inputs[0], inputs[1], request);
    for(auto& input : inputs) input.assign(input.size(), '!');
    if(const auto* error = std::get_if<simnodus::IdealRcError>(&result)) {
        std::cout << "ERR " << static_cast<int>(error->stage) << ' ' << error->code << ' ' << error->offset << ' ' << error->system << ' ' << error->coordinate << '\n';return 1;
    }
    auto owned = std::get<0>(result);
    result = simnodus::IdealRcError{simnodus::IdealRcStage::request, "released", 0};
    const auto& c = *owned;
    std::cout << "OK\n";hex(c.netlist);hex(c.connectivity->source->declaration->sources.lock.syntax->bytes);
    std::cout << c.connectivity->source->declaration->runtime_profile_verified << ' '
        << c.connectivity->source->declaration->sources.topology.simulation_ready << '\n';
    for(const auto& e : c.elements) {
        for(std::size_t i = 0; i < e.path.size(); ++i) std::cout << (i == 0 ? "" : "/") << e.path[i];
        std::cout << ' ' << e.netlist_line << ' ' << e.project_parameter_offset << ' ' << e.project_instance.begin << ' ' << e.project_instance.end
            << ' ' << e.dependency << ' ' << e.resource << ' ' << e.model_source.begin << ' ' << e.model_source.end << '\n';
    }
    std::cout << "NODES\n";
    for(const auto& [name, group] : c.nodes) {
        for(const auto& terminal : group.terminals) {
            std::cout << name << ' ';
            for(std::size_t i = 0; i < terminal.path.size(); ++i) std::cout << (i == 0 ? "" : "/") << terminal.path[i];
            std::cout << ' ' << terminal.terminal << '\n';
        }
    }
}
