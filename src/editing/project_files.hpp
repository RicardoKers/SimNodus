// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// Saving a project with its files (SN-219): the files that File parameters
// name (a board's firmware, a memory chip's contents) are copied into a folder
// next to the project, so it opens and runs on another computer.
#pragma once

#include "library/component_library.hpp"
#include "model/schematic.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace simnodus::editing {

// A file a component's File parameter names (as typed: relative to the project or absolute).
struct ProjectFile {
    model::Id component = 0;
    std::string designator;
    std::string parameter;
    std::string value;
};

std::vector<ProjectFile> project_files(const model::Schematic& schematic, const library::ComponentLibrary& library);

// The schematic with relative file parameters pointing at the same files from
// `new_dir` instead of `old_dir` (absolute when on another drive); the same
// schematic when nothing changes.
model::Schematic rebase_project_files(const model::Schematic& schematic, const library::ComponentLibrary& library,
    const std::filesystem::path& old_dir, const std::filesystem::path& new_dir);

// The folder the files of `project` are copied into: "<name>_files" next to it.
std::filesystem::path files_folder(const std::filesystem::path& project);

struct CopyResult {
    model::Schematic schematic;      // with the parameters pointing at the copies
    std::vector<std::string> copied; // "U1: blink.elf"
    std::vector<std::string> errors; // files that do not exist or could not be copied (their parameters are kept)
};

// Copies every file of `schematic` (relative paths resolved against
// `source_dir`) into files_folder(`target`) and points the parameters at the
// copies, relative to `target`'s folder. Two different files with the same
// name keep apart (the second gets its designator in front); a file named by
// several parts is copied once. Saving the returned schematic is up to the caller.
CopyResult copy_project_files(const model::Schematic& schematic, const library::ComponentLibrary& library,
    const std::filesystem::path& source_dir, const std::filesystem::path& target);

} // namespace simnodus::editing
