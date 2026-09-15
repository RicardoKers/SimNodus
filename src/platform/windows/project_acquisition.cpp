// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#include "platform/project_acquisition_backend.hpp"
#include "platform/windows/resource_handles_internal.hpp"
#include "application/declaration_ingress.hpp"

namespace simnodus::acquisition_platform {
CaptureResult capture(const std::string& root, const std::string& filename)
{
    using namespace resource_platform::detail;
    constexpr auto index = resource_global_error;
    try {
        const auto wide = wide_root(root);
        std::vector<Handle> ancestors;
        ancestors.emplace_back(volume(wide));
        HANDLE parent = ancestors.back().get();
        for(std::size_t start = 3; start < wide.size();) {
            auto end = wide.find(L'\\', start);
            if(end == wide.npos) end = wide.size();
            ancestors.emplace_back(child(parent, wide.substr(start, end - start), true, index));
            parent = ancestors.back().get();
            start = end + 1;
        }
        const auto directory = inspect(parent, true, index);
#ifdef SIMNODUS_ACQUISITION_TEST_HOOKS
        test_boundary("root");
#endif
        auto file = child(parent, std::wstring(filename.begin(), filename.end()), false, index);
        const auto before = inspect(file.get(), false, index);
        if(before.dwVolumeSerialNumber != directory.dwVolumeSerialNumber) fail(ResourceErrorCode::alias, index);
        const auto expected = size(before);
        if(expected > declaration_max_bytes) fail(ResourceErrorCode::budget, index);
#ifdef SIMNODUS_ACQUISITION_TEST_HOOKS
        test_boundary("opened");
#endif
        std::string bytes;
        bytes.reserve(static_cast<std::size_t>(expected));
        std::array<char, 65536> buffer{};
        for(;;) {
            DWORD count = 0;
            const auto amount = static_cast<DWORD>(std::min<std::uint64_t>(buffer.size(), expected + 1 - bytes.size()));
            check(ReadFile(file.get(), buffer.data(), amount, &count, nullptr), index);
            if(count == 0) break;
            if(count > expected - bytes.size()) fail(ResourceErrorCode::size, index);
            bytes.append(buffer.data(), count);
        }
        const auto after = inspect(file.get(), false, index);
        if(identity(before) != identity(after) || size(after) != bytes.size() || bytes.size() != expected)
            fail(ResourceErrorCode::size, index);
#ifdef SIMNODUS_ACQUISITION_TEST_HOOKS
        test_boundary("captured");
#endif
        return bytes;
    } catch(const ResourceError& error) { return error; }
}
}
