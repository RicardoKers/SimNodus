// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#pragma once
#include "application/local_resources.hpp"

namespace simnodus::resource_platform {
ResourceVerification capture(const std::string& root, std::span<const ResourceRequest> requests);
// Test builds alone define and call this boundary; not part of the application API.
#ifdef SIMNODUS_RESOURCE_TEST_HOOKS
void test_boundary(const char* phase, std::size_t index);
#endif
}
