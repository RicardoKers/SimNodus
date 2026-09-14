// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#pragma once
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <vector>
#include <variant>

namespace simnodus {
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
    size, case_sensitive, platform, memory
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
const char* resource_error_name(ResourceErrorCode code) noexcept;
}
