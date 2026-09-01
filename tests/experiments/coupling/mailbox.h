// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#pragma once
#include <stdint.h>

typedef struct {
    uint32_t magic, ticks, input, exti_count, exti_input;
    uint32_t data_check, bss_check, fault, reserved;
} sn_e03_mailbox;

#define SN_E03_MAILBOX_ADDRESS 0x20000000u
#define SN_E03_BOOT_MAGIC 0x534e3033u
#define SN_E03_DATA_COOKIE 0x2468ace0u
