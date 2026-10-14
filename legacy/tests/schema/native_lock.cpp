// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "application/lock_validation.hpp"
#include "application/lock_digest_internal.hpp"
#include <iostream>
#include <type_traits>

int main()
{
    using namespace simnodus;
    static_assert(std::is_const_v<std::remove_reference_t<decltype(*std::get<0>(LockResult{}))>>);
    if(detail::lock_sha256("") != "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"
        || detail::lock_sha256("abc") != "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad") return 1;
    const std::string zero(64, '0');
    const auto inventory = "[{\"bytes\":0,\"path\":\"missing.dll\",\"sha256\":\"" + zero + "\"}]";
    std::string raw = R"({"format":"simnodus-resource-lock","version":"0.1","dependencies":[{"id":"dep","version":"000001.0.000000","origin":{"kind":"external","author":"Declared","reference":"https://invalid.example/no-download","revision":"opaque"},"license":{"identifier":"Unreviewed","notice":"file"},"files":[{"id":"file","path":"missing.dll","bytes":-0,"sha256":")" + zero + R"("}],"content_sha256":")" + detail::lock_sha256(inventory) + R"("}]})";
    const auto original = raw;
    auto result = validate_lock_declaration(raw);
    if(!std::holds_alternative<std::shared_ptr<const LockDeclaration>>(result)) return 1;
    const auto lock = std::get<0>(result);
    raw = "changed";
    result = LockError{"input", 0};
    if(lock->syntax->bytes != original || lock->dependencies != 1 || lock->declared_bytes != 0
        || lock->requests.size() != 1 || lock->requests[0].path != "missing.dll" || lock->requests[0].bytes != 0
        || lock->source_offsets != std::vector<std::size_t>{original.find("{\"id\":\"file\"")}
        || lock->resources_verified || lock->containment_verified || lock->redistribution_verified || lock->simulation_ready) return 1;
    auto invalid = original;
    const auto position = invalid.find("\"missing.dll\"");
    invalid.replace(position, 13, "\"../file\"");
    const auto bad = validate_lock_declaration(invalid);
    const auto* error = std::get_if<LockError>(&bad);
    if(!error || std::string_view(error->code) != "path" || error->offset != position) return 1;
    std::cout << "Lock ownership, request/source identity, signed integer zero, inert missing DLL declaration and SHA-256 vectors passed\n";
}
