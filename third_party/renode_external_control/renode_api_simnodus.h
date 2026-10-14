// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
//
// SimNodus additions to the Renode external control client.
#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "renode_api.h"

// Deadline in milliseconds for each later operation on this connection
// (default 1000, 0 restores it). A timeout retires the connection.
void renode_set_operation_timeout(renode_t *renode, uint32_t milliseconds);

#ifdef __cplusplus
}
#endif
