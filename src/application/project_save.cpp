// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "application/project_save.hpp"
#include "application/project_validation.hpp"
#include "application/resource_path_policy_internal.hpp"
#include "platform/project_save_backend.hpp"
#include <new>

namespace simnodus {
ProjectSaveResult save_new_project(const std::string& root, const std::string& filename, std::string_view bytes)
{
    try {
        const auto validated = validate_project_declaration(bytes);
        if(const auto* error = std::get_if<LockError>(&validated)) return ProjectSaveError{error->code, error->offset};
        if(!resource_policy::root_syntax(root)) return ProjectSaveError{"root"};
        if(!resource_policy::path(filename) || filename.find('/') != filename.npos)
            return ProjectSaveError{"path"};
        const auto& owned = std::get<0>(validated)->sources.lock.syntax->bytes;
        return save_platform::create(root, filename, owned);
    } catch(const std::bad_alloc&) { return ProjectSaveError{"memory"}; }
}
#ifndef _WIN32
ProjectSaveResult save_platform::create(const std::string&, const std::string&, const std::string&)
{
    return ProjectSaveError{"platform"};
}
#endif
}
