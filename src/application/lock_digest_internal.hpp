// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#pragma once
#include <string>
#include <string_view>

namespace simnodus::detail {
// Internal bounded inventory digest, at most 1 MiB; throws LockError on overflow.
// Not an authentication API or a replacement for physical resource verification.
std::string lock_sha256(std::string_view input);
// Experimental managed record integrity only, bounded to 2 MiB. The lock
// inventory entry point above retains its original 1 MiB limit.
std::string managed_record_sha256(std::string_view input);
}
