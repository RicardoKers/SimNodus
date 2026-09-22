// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "adapters/firmware/boot_candidate.hpp"
#include <algorithm>
#include <iostream>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif
int main(int argc, char** argv)
{
    if(argc != 3) return 2;
#ifdef _WIN32
    _setmode(_fileno(stdin), _O_BINARY);
#endif
    std::string input;char chunk[4096];
    while(std::cin && input.size() <= simnodus::firmware::elf_max_bytes) {
        const auto count = std::min(sizeof(chunk), simnodus::firmware::elf_max_bytes + 1 - input.size());
        std::cin.read(chunk, static_cast<std::streamsize>(count));input.append(chunk, static_cast<std::size_t>(std::cin.gcount()));
    }
    const auto expected = input;
    auto result = simnodus::firmware::inspect_boot_candidate(input, argv[2],
        std::string_view(argv[1]) == "sn012" ? simnodus::firmware::BootProfile::sn012_stm32f103c8 : simnodus::firmware::BootProfile::unspecified);
    input.assign("released");
    if(const auto* e = std::get_if<simnodus::firmware::BootCandidateError>(&result)) {
        std::cout << "{\"stage\":\"" << e->stage << "\",\"error\":\"" << e->code << "\",\"offset\":" << e->offset << "}\n";return 1;
    }
    const auto owned = std::get<0>(result);
    result = simnodus::firmware::BootCandidateError{"operation", "released", 0};
    if(owned->elf->bytes != expected) return 2;
    std::cout << "{\"stack\":" << owned->stack << ",\"reset\":" << owned->reset_vector
        << ",\"vector_offset\":" << owned->vector_file_offset << ",\"load_ordered\":"
        << (owned->elf->load_ordered ? "true" : "false") << ",\"bytes\":" << owned->elf->bytes.size() << "}\n";
}
