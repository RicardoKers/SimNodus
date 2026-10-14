// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "application/passive_interface.hpp"
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
    auto result = simnodus::match_passive_interface(inputs[0], inputs[1], inputs[2]);
    for(auto& input : inputs) input.assign(input.size(), '!');
    if(const auto* error = std::get_if<simnodus::PassiveInterfaceError>(&result)) {
        std::cout << "ERR " << static_cast<int>(error->stage) << ' ' << error->code << ' ' << error->offset << '\n'; return 1;
    }
    auto owned = std::get<0>(result);
    result = simnodus::PassiveInterfaceError{simnodus::PassiveInterfaceStage::selection, "released", 0};
    const auto& m = *owned;
    std::cout << "OK\n" << m.descriptor << ' ' << m.asset << ' ' << m.dependency << ' ' << m.resource << '\n'
        << m.terminal_ids[0] << ' ' << m.terminal_ids[1] << ' ' << m.parameter_id << ' ' << m.unit << ' ' << m.minimum << ' ' << m.maximum << '\n'
        << m.model_index << ' ' << m.descriptor_offset << ' ' << m.binding_offset << '\n'
        << m.declaration->resource_interfaces_verified << ' ' << m.declaration->lock.containment_verified << ' ' << m.declaration->topology.simulation_ready << '\n';
    constexpr char digits[] = "0123456789abcdef";
    for(const auto* bytes : {&m.declaration->lock.syntax->bytes, &m.source->bytes}) {
        for(unsigned char c : *bytes) std::cout << digits[c >> 4] << digits[c & 15];
        std::cout << '\n';
    }
}
