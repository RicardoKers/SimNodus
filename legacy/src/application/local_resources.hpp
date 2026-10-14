// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#pragma once
#include <array>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <vector>
#include <variant>

namespace simnodus {
struct PhysicalRootIdentity {
    std::uint64_t volume{};
    std::array<unsigned char, 16> file{};
    bool operator==(const PhysicalRootIdentity&) const = default;
};

struct ResourceRequest {
    std::string dependency;
    std::string resource;
    std::string path;
    std::uint64_t bytes{};
    std::string sha256;
};

struct ResourceSnapshot {
    std::string dependency;
    std::string resource;
    std::string path;
    std::string sha256;
    std::vector<unsigned char> data;
};

enum class ResourceErrorCode {
    id, path, root, budget, hash, filesystem, type, reparse, alias,
    size, case_sensitive, platform, memory, root_identity
};
struct ResourceError {
    ResourceErrorCode code;
    std::size_t index;
    std::uint32_t system_code{};
};
inline constexpr std::size_t resource_global_error = static_cast<std::size_t>(-1);
inline constexpr std::size_t resource_max_files = 256;
inline constexpr std::uint64_t resource_max_file_bytes = 16 * 1024 * 1024;
inline constexpr std::uint64_t resource_max_total_bytes = 64 * 1024 * 1024;
using ResourceSnapshots = std::shared_ptr<const std::vector<ResourceSnapshot>>;
using ResourceVerification = std::variant<ResourceSnapshots, ResourceError>;

// Typed input still needs complete declaration validation by its caller. A
// successful immutable byte inventory grants no interface/trust/execution gate.
ResourceVerification verify_local_resources(const std::string& root_utf8,
    std::span<const ResourceRequest> requests);
// Compare the trusted expected physical root against the same opened root
// handle used to traverse resources. The expected value must come from a
// separately authorized binding, not from this verification operation.
ResourceVerification verify_local_resources(const std::string& root_utf8,
    std::span<const ResourceRequest> requests, const PhysicalRootIdentity& expected_root);
const char* resource_error_name(ResourceErrorCode code) noexcept;
}
