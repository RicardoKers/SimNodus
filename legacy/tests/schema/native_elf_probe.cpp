// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "adapters/firmware/elf_inspection.hpp"
#include <algorithm>
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
    std::string input;
    char chunk[4096];
    while(std::cin && input.size() <= simnodus::firmware::elf_max_bytes) {
        const auto count = std::min(sizeof(chunk), simnodus::firmware::elf_max_bytes + 1 - input.size());
        std::cin.read(chunk, static_cast<std::streamsize>(count));
        input.append(chunk, static_cast<std::size_t>(std::cin.gcount()));
    }
    const auto expected = input;
    auto result = simnodus::firmware::inspect_elf32(input);
    input.assign(input.size(), '!');
    if(const auto* e = std::get_if<simnodus::firmware::ElfInspectionError>(&result)) {
        std::cout << "{\"error\":\"" << e->code << "\",\"offset\":" << e->offset << "}\n";return 1;
    }
    const auto owned = std::get<0>(result);
    result = simnodus::firmware::ElfInspectionError{"released", 0};
    if(owned->bytes != expected) return 2;
    const auto& e = *owned;
    std::cout << "{\"bytes\":" << e.bytes.size() << ",\"machine\":" << e.machine
        << ",\"os_abi\":" << static_cast<unsigned>(e.os_abi) << ",\"abi_version\":" << static_cast<unsigned>(e.abi_version)
        << ",\"entry\":" << e.entry << ",\"flags\":" << e.flags << ",\"section_offset\":" << e.section_table_offset
        << ",\"section_count\":" << e.section_count << ",\"section_names\":" << e.section_names_index << ",\"programs\":[";
    bool first = true;
    for(const auto& p : e.programs) {
        if(!first) std::cout << ',';
        first = false;
        std::cout << '[' << p.type << ',' << p.file_offset << ',' << p.virtual_address << ',' << p.physical_address
            << ',' << p.file_size << ',' << p.memory_size << ',' << p.flags << ',' << p.alignment << ',' << p.header_offset << ']';
    }
    std::cout << "],\"load_ordered\":" << (e.load_ordered ? "true" : "false") << "}\n";
}
