// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "application/passive_numeric.hpp"
#include <iostream>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif
int main()
{
#ifdef _WIN32
    _setmode(_fileno(stdin), _O_BINARY);
#endif
    std::string inputs[3];
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
    auto result = simnodus::bind_passive_numeric(inputs[0], inputs[1], inputs[2]);
    for(auto& input : inputs) input.assign(input.size(), '!');
    if(const auto* error = std::get_if<simnodus::PassiveNumericError>(&result)) {
        std::cout << "ERR " << static_cast<int>(error->stage) << ' ' << error->code << ' ' << error->offset << ' ' << static_cast<int>(error->interface_stage) << '\n'; return 1;
    }
    auto owned = std::get<0>(result);
    result = simnodus::PassiveNumericError{simnodus::PassiveNumericStage::interface, "released", 0};
    const auto& m = *owned;
    std::cout << "OK\n" << m.default_value << '\n' << m.interface->declaration->resource_interfaces_verified << ' '
        << m.interface->declaration->lock.containment_verified << ' ' << m.interface->declaration->topology.simulation_ready << '\n';
    constexpr char digits[] = "0123456789abcdef";
    for(const auto* bytes : {&m.interface->declaration->lock.syntax->bytes, &m.interface->source->bytes}) {
        for(unsigned char c : *bytes) std::cout << digits[c >> 4] << digits[c & 15];
        std::cout << '\n';
    }
    for(const auto& row : m.occurrences) {
        for(std::size_t i = 0; i < row.path.size(); ++i) std::cout << (i == 0 ? "" : "/") << row.path[i];
        std::cout << ' ' << row.definition << ' ' << row.parameter << ' ' << row.pins[0] << ' ' << row.pins[1]
            << ' ' << row.effective.value << ' ' << row.effective.unit << ' ' << row.effective.origin
            << ' ' << row.instance_offset << ' ' << row.effective.source_offset << '\n';
    }
}
