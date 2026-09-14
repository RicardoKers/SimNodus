// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#pragma once
#include <string>
#include <string_view>

namespace simnodus::detail {
// Internal bounded inventory digest, at most 1 MiB; throws LockError on overflow.
// Not an authentication API or a replacement for physical resource verification.
std::string lock_sha256(std::string_view input);
}
