// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "application/project_acquisition.hpp"
#include "application/resource_path_policy_internal.hpp"
#include "platform/project_acquisition_backend.hpp"
#include <new>

namespace simnodus {
BoundProjectAcquisitionResult acquire_project_with_root_identity(
    const std::string& root, const std::string& filename)
{
    try {
        if(!resource_policy::root_syntax(root))
            return ProjectAcquisitionError{ProjectAcquisitionStage::physical, "root"};
        if(!resource_policy::path(filename) || filename.find('/') != filename.npos)
            return ProjectAcquisitionError{ProjectAcquisitionStage::physical, "path"};
        auto captured = acquisition_platform::capture(root, filename);
        if(const auto* error = std::get_if<ResourceError>(&captured))
            return ProjectAcquisitionError{ProjectAcquisitionStage::physical,
                resource_error_name(error->code), 0, error->system_code};
#ifdef SIMNODUS_ACQUISITION_TEST_HOOKS
        acquisition_platform::test_boundary("released");
#endif
        const auto& document = std::get<acquisition_platform::CapturedDocument>(captured);
        const auto graph = load_project_graph(document.bytes);
        if(const auto* error = std::get_if<LockError>(&graph))
            return ProjectAcquisitionError{ProjectAcquisitionStage::declaration, error->code, error->offset};
        return AcquiredProject{std::get<0>(graph), document.root_identity};
    } catch(const std::bad_alloc&) {
        return ProjectAcquisitionError{ProjectAcquisitionStage::physical, "memory"};
    }
}
ProjectAcquisitionResult acquire_project(const std::string& root, const std::string& filename)
{
    auto result = acquire_project_with_root_identity(root, filename);
    if(const auto* error = std::get_if<ProjectAcquisitionError>(&result)) return *error;
    return std::get<AcquiredProject>(std::move(result)).graph;
}
#ifndef _WIN32
acquisition_platform::CaptureResult acquisition_platform::capture(const std::string&, const std::string&)
{
    return ResourceError{ResourceErrorCode::platform, resource_global_error};
}
#endif
}
