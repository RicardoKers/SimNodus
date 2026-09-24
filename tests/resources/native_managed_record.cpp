// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "application/managed_record.hpp"
#include "application/lock_digest_internal.hpp"
#include "application/lock_validation.hpp"
#include <fstream>
#include <iostream>
#include <iterator>

int main(int argc, char** argv)
{
    using namespace simnodus;
    using namespace simnodus::experimental;
    if(argc != 2) return 1;
    std::ifstream input(argv[1], std::ios::binary);
    if(!input) return 1;
    ManagedRecord original;
    original.generation.fill(1); original.document.fill(2); original.commit.fill(3); original.operation.fill(4);
    original.revision = 1;
    original.principal_sid = std::string("\1\1\0\0\0\0\0\5\25\0\0\0", 12);
    original.context.binding.fill(5); original.context.identity.file.fill(6);
    original.context.identity.volume = 7; original.context.locator = "C:/missing-inert-root";
    original.project.assign(std::istreambuf_iterator<char>{input}, {});
    auto encoded = encode_managed_record(original);
    if(!std::holds_alternative<std::string>(encoded)) return 1;
    auto decoded = decode_managed_record(std::get<std::string>(encoded));
    const auto* owned = std::get_if<DecodedManagedRecord>(&decoded);
    if(!owned || owned->record != original) return 1;
    const auto first_digest = owned->record_digest;
    std::get<std::string>(encoded).assign("changed");
    if(owned->record != original) return 1;
    unsigned checks = 2;
    const auto reject = [&](ManagedRecord value) {
        ++checks;
        return std::holds_alternative<ManagedRecordError>(encode_managed_record(value));
    };
    auto changed = original; changed.project = "{}"; if(!reject(changed)) return 1;
    changed = original; changed.project.assign(declaration_max_bytes + 1, ' '); if(!reject(changed)) return 1;
    changed = original; changed.principal_sid.resize(69); if(!reject(changed)) return 1;
    changed = original; changed.context.locator.assign(4097, 'a'); if(!reject(changed)) return 1;
    changed = original; changed.predecessor.revision = 1; if(!reject(changed)) return 1;
    changed = original; changed.generation.fill(0); if(!reject(changed)) return 1;
    changed = original; changed.revision = 65; if(!reject(changed)) return 1;
    changed = original; changed.context.policy = 2; if(!reject(changed)) return 1;
    auto second = original;
    second.revision = 2; second.commit.fill(8); second.operation.fill(9);
    second.predecessor = {original.generation, original.document, 1, original.commit, first_digest,
        {10, original.context.identity.file}};
    const auto second_bytes = encode_managed_record(second);
    if(!std::holds_alternative<std::string>(second_bytes)
        || std::get<std::string>(second_bytes) == std::get<std::string>(encode_managed_record(original))) return 1;
    ++checks;
    changed = second; changed.predecessor.document.fill(99); if(!reject(changed)) return 1;
    changed = second; changed.predecessor.commit = changed.commit; if(!reject(changed)) return 1;
    changed = second; changed.predecessor.identity.file.fill(0); if(!reject(changed)) return 1;
    // Refactoring shared hashing must not increase the old lock inventory bound.
    try { (void)detail::lock_sha256(std::string(declaration_max_bytes + 1, 'x')); return 1; }
    catch(const LockError&) { ++checks; }
    try { (void)detail::managed_record_sha256(std::string(managed_record_max_bytes + 1, 'x')); return 1; }
    catch(const LockError&) { ++checks; }
    // Independent Python hashlib vector at the experimental record hash bound.
    if(detail::managed_record_sha256(std::string(managed_record_max_bytes, 'x'))
        != "6932fd31e5daf4739b9fa78ff777b2831b0995cc1d0b0093cac80601902013bc") return 1;
    ++checks;
    std::cout << checks << " managed record ownership, typed refusal and budget checks passed\n";
}
