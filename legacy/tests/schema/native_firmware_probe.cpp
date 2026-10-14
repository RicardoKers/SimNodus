// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "application/firmware_inspection.hpp"
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
int main()
{
#ifdef _WIN32
    _setmode(_fileno(stdin), _O_BINARY);
#endif
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
    auto result = simnodus::inspect_project_firmware(inputs[0], inputs[1], inputs[2]);
    for(auto& input : inputs) input.assign("released");
    if(const auto* e = std::get_if<simnodus::FirmwareInspectionError>(&result)) {
        std::cout << "ERR " << static_cast<int>(e->stage) << ' ' << e->code << ' ' << e->offset << ' ' << e->coordinate << '\n';return 1;
    }
    const auto owned = std::get<0>(result);
    result = simnodus::FirmwareInspectionError{simnodus::FirmwareInspectionStage::selection, "released"};
    // Test-only barrier; the caller may replace its fixture after capture.
    std::cout << "CAPTURED\n" << std::flush;
    if(std::cin.get() != 'c') return 2;
    const auto& r = *owned;const auto& snapshot = (*r.resources)[r.resource_index];
    std::cout << "OK\n" << r.firmware_id << ' ' << r.declared_architecture << ' ' << r.elf->machine << ' ' << r.elf->load_ordered << '\n';
    std::cout << snapshot.dependency << ' ' << snapshot.resource << ' ' << snapshot.sha256 << '\n';
    std::cout << r.firmware_offset << ' ' << r.architecture_offset << ' ' << r.resource_index << '\n';
    std::cout << r.declaration->firmware_verified << ' ' << r.declaration->runtime_profile_verified << ' '
        << r.declaration->sources.topology.simulation_ready << '\n';
    hex(r.declaration->sources.lock.syntax->bytes);hex(r.elf->bytes);
    if(snapshot.data.size() != r.elf->bytes.size()) return 2;
    for(std::size_t i = 0; i < snapshot.data.size(); ++i)
        if(snapshot.data[i] != static_cast<unsigned char>(r.elf->bytes[i])) return 2;
}
