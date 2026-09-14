// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "application/binding_validation.hpp"
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
    const auto result = simnodus::validate_binding_declaration(raw);
    if(const auto* error = std::get_if<simnodus::ParameterError>(&result)) {
        std::cout << "{\"error\":\"" << error->code << "\",\"offset\":" << error->offset << "}\n";
        return 1;
    }
    const auto& binding = *std::get<0>(result);
    const auto& snapshot = binding.parameters;
    std::cout << "{\"stats\":{\"declared_entities\":" << snapshot.topology.declared_entities
        << ",\"root_depth\":" << snapshot.topology.root_depth << ",\"root_expanded_instances\":" << snapshot.topology.root_expanded_instances
        << ",\"parameter_entries\":" << snapshot.parameter_entries << ",\"resolved_entries\":" << snapshot.resolved_entries
        << ",\"descriptor_entries\":" << binding.descriptor_entries << ",\"symbol_bindings\":" << binding.symbol_bindings
        << ",\"model_bindings\":" << binding.model_bindings << ",\"simulation_ready\":" << (binding.simulation_ready ? "true" : "false") << "},\"parameters\":";
    std::cout << "{\"status\":\"valid-parameters-only\",\"declared_entities\":" << snapshot.topology.declared_entities
        << ",\"root_depth\":" << snapshot.topology.root_depth << ",\"root_expanded_instances\":" << snapshot.topology.root_expanded_instances
        << ",\"parameter_entries\":" << snapshot.parameter_entries << ",\"resolved_entries\":" << snapshot.resolved_entries << ",\"instances\":[";
    // Only validated ASCII IDs, closed units/origins and decimal strings are emitted.
    bool first = true;
    for(const auto& row : snapshot.instances) {
        if(!first) std::cout << ',';
        first = false;
        std::cout << "{\"path\":[";
        bool first_path = true;
        for(const auto& part : row.path) {
            if(!first_path) std::cout << ',';
            first_path = false;
            std::cout << '"' << part << '"';
        }
        std::cout << "],\"definition\":\"" << row.definition << "\",\"parameters\":{";
        bool first_parameter = true;
        for(const auto& [id, value] : row.parameters) {
            if(!first_parameter) std::cout << ',';
            first_parameter = false;
            std::cout << '"' << id << "\":{\"value\":\"" << value.value << "\",\"unit\":\"" << value.unit
                << "\",\"origin\":\"" << value.origin << "\"}";
        }
        std::cout << "}}";
    }
    std::cout << "]}}\n";
}
