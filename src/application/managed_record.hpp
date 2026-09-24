// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <variant>

namespace simnodus::experimental {
using ManagedId = std::array<unsigned char, 16>;
using ManagedDigest = std::array<unsigned char, 32>;
struct ManagedIdentity {
    std::uint64_t volume{};
    ManagedId file{};
    bool operator==(const ManagedIdentity&) const = default;
};
struct ManagedToken {
    ManagedId generation{}, document{};
    std::uint32_t revision{};
    ManagedId commit{};
    ManagedDigest digest{};
    ManagedIdentity identity{};
    bool operator==(const ManagedToken&) const = default;
};
struct ManagedContext {
    ManagedId binding{};
    ManagedIdentity identity{};
    // Policy 1 means the existing local-root capture policy, never a capability.
    std::uint32_t policy = 1;
    std::string locator;
    bool operator==(const ManagedContext&) const = default;
};
struct ManagedRecord {
    ManagedId generation{}, document{}, commit{}, operation{};
    std::uint32_t revision{};
    ManagedToken predecessor{};
    // Binary SID, copied from authenticated operation input by a future caller.
    // This codec cannot authenticate it or observe either physical identity.
    std::string principal_sid;
    ManagedContext context;
    std::string project;
    bool operator==(const ManagedRecord&) const = default;
};
enum class ManagedRecordError { size, format, fields, context, project, digest, memory };
struct DecodedManagedRecord {
    ManagedRecord record;
    ManagedDigest request_digest{}, record_digest{};
};
using ManagedEncoding = std::variant<std::string, ManagedRecordError>;
using ManagedDecoding = std::variant<DecodedManagedRecord, ManagedRecordError>;
inline constexpr std::size_t managed_record_max_bytes = 2 * 1024 * 1024;
inline constexpr std::size_t managed_context_max_bytes = 16 * 1024;
inline constexpr std::uint32_t managed_revision_limit = 64;
// Inert, owned canonical encoding. Borrowed inputs must remain stable during calls.
// No filesystem, chain recovery, authentication, execution or publication authority.
ManagedEncoding encode_managed_record(const ManagedRecord& record);
ManagedDecoding decode_managed_record(std::string_view bytes);
}
