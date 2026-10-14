// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#pragma once
#include "application/local_resources.hpp"

namespace simnodus::acquisition_platform {
struct CapturedDocument {
    std::string bytes;
    PhysicalRootIdentity root_identity;
};
using CaptureResult = std::variant<CapturedDocument, ResourceError>;
CaptureResult capture(const std::string& root, const std::string& filename);
#ifdef SIMNODUS_ACQUISITION_TEST_HOOKS
void test_boundary(const char* phase);
#endif
}
