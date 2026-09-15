// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "platform/local_resources_backend.hpp"
#include "platform/windows/resource_handles_internal.hpp"
#include <windows.h>
#include <winternl.h>
#include <bcrypt.h>
#include <algorithm>
#include <array>
#include <map>
#include <set>
#include <tuple>
#include <utility>

namespace simnodus::resource_platform {
using namespace detail;
namespace {
std::string sha256(const std::vector<unsigned char>& data, std::size_t index)
{
    std::array<unsigned char, 32> digest{};
    const auto result = BCryptHash(BCRYPT_SHA256_ALG_HANDLE, nullptr, 0,
        const_cast<unsigned char*>(data.data()), static_cast<ULONG>(data.size()), digest.data(),
        static_cast<ULONG>(digest.size()));
    if(result < 0) fail(ResourceErrorCode::filesystem, index, static_cast<DWORD>(result));
    std::string hex;
    for(auto value : digest) {
        hex += "0123456789abcdef"[value >> 4];
        hex += "0123456789abcdef"[value & 15];
    }
    return hex;
}
}

ResourceVerification capture(const std::string& root, std::span<const ResourceRequest> requests)
{
    try {
        const auto wide = wide_root(root);
        std::vector<Handle> ancestors;
        ancestors.emplace_back(volume(wide));
        HANDLE selected = ancestors.back().get();
        for(std::size_t start = 3; start < wide.size();) {
            auto end = wide.find(L'\\', start);
            if(end == wide.npos) end = wide.size();
            ancestors.emplace_back(child(selected, wide.substr(start, end - start), true, resource_global_error));
            selected = ancestors.back().get();
            start = end + 1;
        }
        const auto root_info = inspect(selected, true, resource_global_error);
#ifdef SIMNODUS_RESOURCE_TEST_HOOKS
        test_boundary("root", resource_global_error);
#endif
        std::map<std::string, HANDLE> directories;
        std::set<Identity> identities;
        std::vector<ResourceSnapshot> snapshots;
        std::uint64_t total = 0;
        for(std::size_t i = 0; i < requests.size(); ++i) {
            const auto& request = requests[i];
            HANDLE parent = selected;
            std::size_t start = 0;
            for(auto end = request.path.find('/'); end != request.path.npos; end = request.path.find('/', start)) {
                const auto key = request.path.substr(0, end);
                auto existing = directories.find(key);
                if(existing == directories.end()) {
                    const auto part = request.path.substr(start, end - start);
                    ancestors.emplace_back(child(parent, std::wstring(part.begin(), part.end()), true, i));
                    existing = directories.emplace(key, ancestors.back().get()).first;
                }
                parent = existing->second;
                start = end + 1;
            }
            const auto part = request.path.substr(start);
            auto file = child(parent, std::wstring(part.begin(), part.end()), false, i);
            const auto before = inspect(file.get(), false, i);
            if(before.dwVolumeSerialNumber != root_info.dwVolumeSerialNumber
                || !identities.insert(identity(before)).second) fail(ResourceErrorCode::alias, i);
            if(size(before) > resource_max_file_bytes) fail(ResourceErrorCode::budget, i);
            if(size(before) != request.bytes) fail(ResourceErrorCode::size, i);
#ifdef SIMNODUS_RESOURCE_TEST_HOOKS
            test_boundary("opened", i);
#endif
            std::vector<unsigned char> data;
            data.reserve(static_cast<std::size_t>(request.bytes));
            std::array<unsigned char, 65536> buffer{};
            for(;;) {
                DWORD count = 0;
                auto amount = static_cast<DWORD>(std::min<std::uint64_t>(buffer.size(), request.bytes + 1 - data.size()));
                check(ReadFile(file.get(), buffer.data(), amount, &count, nullptr), i);
                if(count == 0) break;
                if(count > request.bytes - data.size()) fail(ResourceErrorCode::size, i);
                data.insert(data.end(), buffer.begin(), buffer.begin() + count);
            }
            const auto after = inspect(file.get(), false, i);
            if(identity(before) != identity(after) || size(after) != data.size() || data.size() != request.bytes)
                fail(ResourceErrorCode::size, i);
            total += data.size();
            if(total > resource_max_total_bytes) fail(ResourceErrorCode::budget, i);
            const auto digest = sha256(data, i);
            if(digest != request.sha256) fail(ResourceErrorCode::hash, i);
            snapshots.push_back({request.dependency, request.resource, request.path, digest, std::move(data)});
        }
#ifdef SIMNODUS_RESOURCE_TEST_HOOKS
        test_boundary("captured", resource_global_error);
#endif
        return std::make_shared<const std::vector<ResourceSnapshot>>(std::move(snapshots));
    } catch(const ResourceError& error) {
        return error;
    }
}
}
