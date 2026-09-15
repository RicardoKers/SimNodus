// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "application/project_graph.hpp"
#include <iostream>
#include "graph_probe_output.hpp"
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
    const auto result = simnodus::load_project_graph(raw);
    if(const auto* error = std::get_if<simnodus::LockError>(&result)) {
        std::cout << "{\"error\":\"" << error->code << "\",\"offset\":" << error->offset << "}\n";
        return 1;
    }
    const auto& loaded = *std::get<0>(result);
    const auto& project = *loaded.declaration;
    std::cout << '{';
    graph_probe::write(loaded);
    const auto& links = project.sources;
    const auto& lock = links.lock;
    const auto& b = links.topology;
    const auto& p = b.parameters;
    std::cout << std::boolalpha << "\"project_id\":\"" << project.project_id << "\",\"targets\":" << project.targets
        << ",\"project_entries\":" << project.project_entries << ",\"runtime_profile_verified\":" << project.runtime_profile_verified
        << ",\"firmware_verified\":" << project.firmware_verified << ",\"descriptors_linked\":" << links.descriptors_linked << ",\"assets\":" << links.assets
        << ",\"dependencies\":" << lock.dependencies << ",\"resources\":" << lock.requests.size() << ",\"declared_bytes\":" << lock.declared_bytes
        << ",\"resources_verified\":" << lock.resources_verified << ",\"containment_verified\":" << lock.containment_verified
        << ",\"redistribution_verified\":" << lock.redistribution_verified << ",\"simulation_ready\":" << lock.simulation_ready
        << ",\"resource_interfaces_verified\":" << links.resource_interfaces_verified << ",\"topology_stats\":{\"declared_entities\":" << p.topology.declared_entities
        << ",\"parameter_entries\":" << p.parameter_entries << ",\"resolved_entries\":" << p.resolved_entries
        << ",\"root_depth\":" << p.topology.root_depth << ",\"root_expanded_instances\":" << p.topology.root_expanded_instances
        << ",\"descriptor_entries\":" << b.descriptor_entries << ",\"symbol_bindings\":" << b.symbol_bindings
        << ",\"model_bindings\":" << b.model_bindings << ",\"simulation_ready\":" << b.simulation_ready << "}}\n";
}
