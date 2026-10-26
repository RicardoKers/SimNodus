// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "editing/project_files.hpp"

#include "editing/operations.hpp"

#include <map>
#include <system_error>

namespace simnodus::editing {
namespace {

std::filesystem::path from_utf8(const std::string& text)
{
    return std::filesystem::path(std::u8string(text.begin(), text.end()));
}

std::string to_utf8(const std::filesystem::path& path)
{
    const auto text = path.generic_u8string();
    return std::string(text.begin(), text.end());
}

} // namespace

std::vector<ProjectFile> project_files(const model::Schematic& schematic, const library::ComponentLibrary& library)
{
    std::vector<ProjectFile> files;
    for (const auto& c : schematic.components()) {
        const auto* d = library.find(c.type);
        if (!d) {
            continue;
        }
        for (const auto& p : d->parameters) {
            const auto it = c.parameters.find(p.name);
            if (p.kind == library::Parameter::Kind::File && it != c.parameters.end() && !it->second.empty()) {
                files.push_back({c.id, c.designator, p.name, it->second});
            }
        }
    }
    return files;
}

model::Schematic rebase_project_files(const model::Schematic& schematic, const library::ComponentLibrary& library,
    const std::filesystem::path& old_dir, const std::filesystem::path& new_dir)
{
    model::Schematic result = schematic;
    const auto from = std::filesystem::absolute(old_dir).lexically_normal();
    const auto to = std::filesystem::absolute(new_dir).lexically_normal();
    if (old_dir.empty() || from == to) {
        return result;
    }
    for (const auto& file : project_files(schematic, library)) {
        const auto path = from_utf8(file.value);
        if (!path.is_relative()) {
            continue;
        }
        const auto absolute = (from / path).lexically_normal();
        auto moved = absolute.lexically_relative(to);
        if (moved.empty()) { // another drive
            moved = absolute;
        }
        set_parameter(result, file.component, file.parameter, to_utf8(moved)).apply(result);
    }
    return result;
}

std::filesystem::path files_folder(const std::filesystem::path& project)
{
    auto name = project.stem();
    name += "_files";
    return project.parent_path() / name;
}

CopyResult copy_project_files(const model::Schematic& schematic, const library::ComponentLibrary& library,
    const std::filesystem::path& source_dir, const std::filesystem::path& target)
{
    CopyResult result{schematic, {}, {}};
    const auto project = std::filesystem::absolute(target);
    const auto folder = files_folder(project);
    std::map<std::filesystem::path, std::filesystem::path> copies; // source -> copy
    std::map<std::filesystem::path, std::filesystem::path> taken;  // copy name -> source
    for (const auto& file : project_files(schematic, library)) {
        auto source = from_utf8(file.value);
        if (source.is_relative()) {
            source = source_dir / source;
        }
        std::error_code ec;
        source = std::filesystem::weakly_canonical(source, ec);
        if (ec || !std::filesystem::is_regular_file(source, ec)) {
            result.errors.push_back(file.designator + ": " + file.value + " does not exist");
            continue;
        }
        auto copy = copies.find(source);
        if (copy == copies.end()) {
            auto name = source.filename();
            if (const auto other = taken.find(name); other != taken.end() && other->second != source) {
                name = from_utf8(file.designator + "_") += source.filename();
            }
            const auto destination = folder / name;
            std::filesystem::create_directories(folder, ec);
            // The project may already be saved with its files: copying a file onto itself is a no-op.
            if (!ec && !std::filesystem::equivalent(source, destination, ec)) {
                ec.clear();
                std::filesystem::copy_file(source, destination, std::filesystem::copy_options::overwrite_existing, ec);
            } else {
                ec.clear();
            }
            if (ec) {
                result.errors.push_back(file.designator + ": " + file.value + " could not be copied (" + ec.message() + ")");
                continue;
            }
            taken[name] = source;
            copy = copies.emplace(source, destination).first;
        }
        const auto relative = copy->second.lexically_relative(project.parent_path());
        set_parameter(result.schematic, file.component, file.parameter, to_utf8(relative)).apply(result.schematic);
        result.copied.push_back(file.designator + ": " + to_utf8(copy->second.filename()));
    }
    return result;
}

} // namespace simnodus::editing
