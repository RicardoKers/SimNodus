// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <variant>

namespace simnodus {
struct ProjectSaveReceipt { std::size_t bytes; };
struct ProjectSaveError {
    const char* code;
    std::size_t offset = 0;
    std::uint32_t system_code = 0;
    bool temporary_cleanup_failed = false;
};
using ProjectSaveResult = std::variant<ProjectSaveReceipt, ProjectSaveError>;
// Explicit create-only persistence. Validate/capture project 0.1, then publish
// its exact owned bytes in an existing selected directory. Never overwrite.
// Borrowed arguments must remain stable during this call. No resources execute.
ProjectSaveResult save_new_project(const std::string& root_utf8,
    const std::string& filename, std::string_view bytes);
}
