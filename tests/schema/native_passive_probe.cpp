// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "adapters/ngspice/passive_source.hpp"
#include <iostream>
#include <iterator>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif
int main()
{
#ifdef _WIN32
    _setmode(_fileno(stdin), _O_BINARY);
#endif
    std::string input{std::istreambuf_iterator<char>(std::cin), {}};
    auto result = simnodus::spice::inspect_passive_source(input);
    input.assign(input.size(), '!');
    if(const auto* error = std::get_if<simnodus::spice::PassiveSourceError>(&result)) {
        std::cout << "ERR " << error->code << ' ' << error->offset << '\n';
        return 1;
    }
    auto owned = std::get<std::shared_ptr<const simnodus::spice::PassiveSource>>(result);
    result = simnodus::spice::PassiveSourceError{"released", 0};
    std::cout << "OK\n";
    constexpr char digits[] = "0123456789abcdef";
    for(unsigned char c : owned->bytes) std::cout << digits[c >> 4] << digits[c & 15];
    std::cout << '\n';
    for(const auto& model : owned->subcircuits)
        std::cout << model.name << ' ' << model.terminals[0] << ' ' << model.terminals[1]
                  << ' ' << model.parameter << ' ' << model.default_literal << ' '
                  << (model.kind == simnodus::spice::PassiveKind::resistor ? "R" : "C")
                  << ' ' << model.begin << ' ' << model.end << '\n';
}
