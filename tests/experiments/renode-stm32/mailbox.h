// Copyright (c) 2026 Ricardo Kerschbaumer
// SPDX-License-Identifier: MIT
#pragma once
#include <stdint.h>

// Firmware owns results; the host owns command, mode, and drive only.
typedef struct {
    uint32_t magic, ticks, input, exti_count, exti_input;
    uint32_t data_check, bss_check, fault;
    uint32_t command, acknowledged, mode, drive;
    uint32_t mode_readback, systick_control, systick_reload, reserved;
} sn_mailbox;
#define SN_MAILBOX_ADDRESS 0x20000000u
#define SN_BOOT_MAGIC 0x534e3032u
#define SN_DATA_COOKIE 0x13579bdfu
