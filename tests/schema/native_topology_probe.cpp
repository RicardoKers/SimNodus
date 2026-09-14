// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "application/topology_validation.hpp"
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
    std::string raw(simnodus::declaration_max_bytes + 1, '\0');
    std::cin.read(raw.data(), static_cast<std::streamsize>(raw.size()));
    raw.resize(static_cast<std::size_t>(std::cin.gcount()));
    const auto result = simnodus::validate_topology_declaration(raw);
    if(const auto* error = std::get_if<simnodus::TopologyError>(&result)) {
        std::cout << "ERR " << simnodus::topology_error_name(error->code) << ' ' << error->offset << '\n';
        return 1;
    }
    const auto& topology = std::get<simnodus::TopologyDeclaration>(result);
    std::cout << "OK " << topology.declared_entities << ' ' << topology.root_depth << ' ' << topology.root_expanded_instances << '\n';
}
