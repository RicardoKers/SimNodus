// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// SimNodus project files (.snx, PRJ-01..PRJ-03): one human-readable JSON
// document. Reading a project only parses data: it never executes code,
// loads libraries or touches the network. Format: docs/architecture/PROJECT_FORMAT.md.
#pragma once

#include "model/schematic.hpp"

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace simnodus::io {

// Format 2 adds the user's blocks (ADR 0098); projects without blocks are written as format 1.
constexpr int project_format_version = 2;

struct LoadResult {
    std::optional<model::Schematic> schematic; // empty on error
    std::vector<std::string> errors;           // why the file could not be read
    std::vector<std::string> warnings;         // readable, but something was skipped or adjusted
};

std::string to_json_text(const model::Schematic& schematic);
LoadResult from_json_text(const std::string& text);

// Selected elements only (clipboard), with the blocks they use; IDs are kept and remapped on paste.
std::string elements_to_json_text(const std::vector<model::Element>& elements,
    const std::map<std::string, model::Block>& blocks = {});
std::optional<std::vector<model::Element>> elements_from_json_text(const std::string& text, std::string& error,
    std::map<std::string, model::Block>* blocks = nullptr);

LoadResult load_project(const std::filesystem::path& file);

struct SaveResult {
    bool ok = false;
    std::string error;
    std::filesystem::path backup; // previous version, if there was one
};

// Writes a temporary file next to `file`, keeps the previous version as
// "<name>.snx.bak" and then replaces `file`, so that a failed save never
// destroys the existing project.
SaveResult save_project(const model::Schematic& schematic, const std::filesystem::path& file);

} // namespace simnodus::io
