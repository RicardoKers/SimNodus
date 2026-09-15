// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "application/local_resources.hpp"
#include "application/resource_path_policy_internal.hpp"
#include "platform/local_resources_backend.hpp"
#include <algorithm>
#include <new>
#include <set>
#include <string_view>

namespace simnodus {
using namespace resource_policy;

ResourceVerification verify_local_resources(const std::string& root, std::span<const ResourceRequest> requests)
{
    try {
        if(requests.empty() || requests.size() > resource_max_files)
            return ResourceError{ResourceErrorCode::budget, resource_global_error};
        std::set<std::string> dependencies, paths;
        std::set<std::pair<std::string, std::string>> ids;
        std::uint64_t total = 0;
        for(std::size_t i = 0; i < requests.size(); ++i) {
            const auto& r = requests[i];
            if(!id(r.dependency) || !id(r.resource) || !ids.emplace(r.dependency, r.resource).second)
                return ResourceError{ResourceErrorCode::id, i};
            dependencies.insert(r.dependency);
            if(!path(r.path) || !paths.insert(lower(r.path)).second)
                return ResourceError{ResourceErrorCode::path, i};
            if(r.bytes > resource_max_file_bytes || r.bytes > resource_max_total_bytes - total
                || dependencies.size() > 32) return ResourceError{ResourceErrorCode::budget, i};
            total += r.bytes;
            if(r.sha256.size() != 64 || !std::all_of(r.sha256.begin(), r.sha256.end(),
                [](char c) { return digit(c) || (c >= 'a' && c <= 'f'); }))
                return ResourceError{ResourceErrorCode::hash, i};
        }
        for(const auto& p : paths) {
            for(auto end = p.find('/'); end != p.npos; end = p.find('/', end + 1))
                if(paths.contains(p.substr(0, end))) return ResourceError{ResourceErrorCode::path, resource_global_error};
        }
        if(!root_syntax(root)) return ResourceError{ResourceErrorCode::root, resource_global_error};
        return resource_platform::capture(root, requests);
    } catch(const std::bad_alloc&) {
        return ResourceError{ResourceErrorCode::memory, resource_global_error};
    }
}

const char* resource_error_name(ResourceErrorCode code) noexcept
{
    switch(code) {
    case ResourceErrorCode::id: return "id";
    case ResourceErrorCode::path: return "path";
    case ResourceErrorCode::root: return "root";
    case ResourceErrorCode::budget: return "budget";
    case ResourceErrorCode::hash: return "hash";
    case ResourceErrorCode::filesystem: return "filesystem";
    case ResourceErrorCode::type: return "type";
    case ResourceErrorCode::reparse: return "reparse";
    case ResourceErrorCode::alias: return "alias";
    case ResourceErrorCode::size: return "size";
    case ResourceErrorCode::case_sensitive: return "case_sensitive";
    case ResourceErrorCode::platform: return "platform";
    case ResourceErrorCode::memory: return "memory";
    }
    return "unknown";
}

#ifndef _WIN32
ResourceVerification resource_platform::capture(const std::string&, std::span<const ResourceRequest>)
{
    return ResourceError{ResourceErrorCode::platform, resource_global_error};
}
#endif
}
