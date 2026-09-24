// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "application/managed_record.hpp"
#include <fstream>
#include <iostream>
#include <iterator>

int main()
{
    using namespace simnodus::experimental;
    std::string path;
    while(std::getline(std::cin, path)) {
        std::ifstream input(path, std::ios::binary | std::ios::ate);
        if(!input || input.tellg() > static_cast<std::streamoff>(managed_record_max_bytes + 1)) return 2;
        input.seekg(0);
        const std::string bytes(std::istreambuf_iterator<char>{input}, {});
        const auto result = decode_managed_record(bytes);
        if(const auto* error = std::get_if<ManagedRecordError>(&result)) {
            std::cout << "reject " << static_cast<unsigned>(*error) << '\n';
        } else {
            const auto encoded = encode_managed_record(std::get<DecodedManagedRecord>(result).record);
            const auto* exact = std::get_if<std::string>(&encoded);
            if(!exact || *exact != bytes) return 3;
            std::cout << "accept\n";
        }
    }
}
