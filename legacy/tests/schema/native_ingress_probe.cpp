// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "application/declaration_ingress.hpp"
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
    const auto result = simnodus::capture_declaration_syntax(raw);
    if(const auto* error = std::get_if<simnodus::IngressError>(&result)) {
        std::cout << "ERR " << static_cast<int>(error->code) << ' ' << error->offset << '\n';
        return 1;
    }
    std::cout << "OK\n";
    for(const auto& token : std::get<0>(result)->tokens) {
        std::cout << static_cast<int>(token.kind) << ' ' << token.begin << ' ' << token.end << ' ' << token.next << ' ';
        for(const auto c : token.decoded) {
            const auto byte = static_cast<unsigned char>(c);
            std::cout << "0123456789abcdef"[byte >> 4] << "0123456789abcdef"[byte & 15];
        }
        std::cout << '\n';
    }
}
