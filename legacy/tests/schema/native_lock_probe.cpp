// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "application/lock_validation.hpp"
#include "application/lock_digest_internal.hpp"
#include <iostream>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

int main(int argc, char** argv)
{
#ifdef _WIN32
    _setmode(_fileno(stdin), _O_BINARY);
#endif
    std::string raw(simnodus::declaration_max_bytes + 1, '\0');
    std::cin.read(raw.data(), static_cast<std::streamsize>(raw.size()));
    raw.resize(static_cast<std::size_t>(std::cin.gcount()));
    if(argc == 2 && std::string_view(argv[1]) == "--sha") {
        try { std::cout << simnodus::detail::lock_sha256(raw) << '\n'; return 0; }
        catch(const simnodus::LockError&) { return 1; }
    }
    const auto result = simnodus::validate_lock_declaration(raw);
    if(const auto* error = std::get_if<simnodus::LockError>(&result)) {
        std::cout << "{\"error\":\"" << error->code << "\",\"offset\":" << error->offset << "}\n";
        return 1;
    }
    const auto& lock = *std::get<0>(result);
    std::cout << std::boolalpha << "{\"stats\":{\"dependencies\":" << lock.dependencies << ",\"resources\":" << lock.requests.size()
        << ",\"declared_bytes\":" << lock.declared_bytes << ",\"resources_verified\":" << lock.resources_verified
        << ",\"containment_verified\":" << lock.containment_verified << ",\"redistribution_verified\":" << lock.redistribution_verified
        << ",\"simulation_ready\":" << lock.simulation_ready << "},\"requests\":[";
    bool first = true;
    // Only validated ASCII IDs, paths and hashes are emitted, without metadata text.
    for(const auto& row : lock.requests) {
        if(!first) std::cout << ',';
        first = false;
        std::cout << "{\"dependency\":\"" << row.dependency << "\",\"resource\":\"" << row.resource << "\",\"path\":\"" << row.path
            << "\",\"bytes\":" << row.bytes << ",\"sha256\":\"" << row.sha256 << "\"}";
    }
    std::cout << "]}\n";
}
