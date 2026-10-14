// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#pragma once
#include "application/project_save.hpp"

namespace simnodus::save_platform {
// Internal: caller validated root/name syntax and owns validated project bytes.
ProjectSaveResult create(const std::string& root, const std::string& filename, const std::string& bytes);
#ifdef SIMNODUS_SAVE_TEST_HOOKS
// Explicit test transport only; never compiled into the production library.
bool test_boundary(const char* phase);
#endif
}
